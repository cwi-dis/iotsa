#ifndef _IOTSA_H_
#define _IOTSA_H_

#include "iotsaVersion.h"
#include "iotsaBuildOptions.h"
#include <Print.h>
#include <functional>
#include <vector>

// How long a maintenance mode (config / OTA) stays open before auto-expiring, in
// seconds. Seeds IotsaController::_modeTimeout; config.cfg's "rebootTimeout" key
// overrides at runtime. Defined here (ahead of the includes below) so
// iotsaController.h can use it as the member's default.
#define CONFIGURATION_MODE_TIMEOUT 300

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "iotsaWebServer.h"
#include "iotsaConfig.h"
#include "iotsaStatus.h"
#include "iotsaBreadcrumbs.h"
#include "iotsaController.h"
#include "iotsaDeadline.h"
#include <ArduinoJson.h>

// IotsaSerial and the IOTSA_LOG* macros (cwi-dis/iotsa#182).
#include "iotsaLog.h"

class IotsaBaseModule;
class IotsaConfigMod;
// The shared HTTP(S) transport module (owns the actual IotsaWebServer instance) --
// a peer service mod like IotsaCoapServiceMod/IotsaHpsServiceMod, not privileged
// IotsaApplication inheritance, see cwi-dis/iotsa#207. Full definition lives in
// iotsaHttpServer.h; only needed here as a forward declaration, for the friend
// declarations below (IotsaHttpServiceMod reads IotsaApplication::firstModule/
// firstEarlyModule directly to render the root page).
class IotsaHttpServiceMod;

//
// Operations allowed via the API
//
typedef enum IotsaApiOperation {
  IOTSA_API_GET,
  IOTSA_API_PUT,
  IOTSA_API_POST,
  IOTSA_API_DELETE
} IotsaApiOperation;

class IotsaAuthenticationProvider;

class IotsaApplication {
  friend class IotsaBaseModule;
  friend class IotsaAuthMod;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaHttpServiceMod;
  friend class IotsaRunmodeMod;
public:
  IotsaApplication(const char *_title);
  // Explicitly disable copy constructor and assignment
  IotsaApplication(const IotsaApplication& that) = delete;
  IotsaApplication& operator=(const IotsaApplication& that) = delete;

  void addMod(IotsaBaseModule *mod);
  void addModEarly(IotsaBaseModule *mod);
  // The application's one authentication provider, used by every module, the
  // standard ones included (cwi-dis/iotsa#85, #284). Call before setup(). With
  // a stack (e.g. IotsaCapabilityMod over IotsaUserMod) pass the top. Without
  // setAuth(), getAuth() returns a provider that allows everything.
  void setAuth(IotsaAuthenticationProvider *auth) { _auth = auth; }
  IotsaAuthenticationProvider *getAuth();
  void setup();
  void lateSetup();
  void loop();
  // Call just before deliberately holding loop() for a long time (an OTA
  // transfer): runs one more pass over the modules' loop(), so e.g. status
  // renderers show the latest state, which then stays visible while loop() is
  // blocked (cwi-dis/iotsa#259). The module whose loop() is calling this is
  // skipped. Does nothing when nested, or outside the loop task.
  void aboutToBlock();
  // Code that runs outside the loop task (NimBLE callbacks, other tasks) must
  // not touch module state: it latches a volatile, or hands the work to the
  // loop task with one of these (cwi-dis/iotsa#236).
  // postToLoop(): run fn in the loop task at the start of the next loop()
  // pass, return immediately. false if the queue is full (fn is dropped).
  // runInLoop(): the same, but wait until fn has run, at most timeoutMs.
  // false on timeout, and then fn is guaranteed never to run, so fn may
  // capture the caller's locals by reference. Called from the loop task
  // itself, both just call fn. On ESP8266 there are no other tasks.
  static bool postToLoop(std::function<void()> fn);
  static bool runInLoop(std::function<void()> fn, uint32_t timeoutMs);
  static bool inLoopTask();
#ifdef IOTSA_HAS_WEBSERVER
  // Convenience for app-level sketch code (e.g. tests/KitchenSink, examples/Hello)
  // that registers its own raw handler outside of any module method,
  // and for any module that isn't itself an API-having module with a webHandler()
  // (web-server-extension modules like IotsaFilesUploadMod/IotsaLoggerMod/
  // IotsaSimpleMod, and a module's own registrations that fall outside its page,
  // like IotsaConfigMod's cert upload, cwi-dis/iotsa#221) -- forwards to the shared
  // IotsaHttpServiceMod, set once in the constructor.
  // API-having modules' webHandler() bodies reach the shared server through their
  // own IotsaApiServiceWeb link instead (e.g. `api.webService->server`); see
  // cwi-dis/iotsa#207/#211.
  IotsaWebServer *server = nullptr;
#endif
protected:
  IotsaBaseModule *firstModule;
  IotsaBaseModule *firstEarlyModule;
  String title;
  IotsaAuthenticationProvider *_auth = nullptr;
  bool _haveAuthModule = false;  // an IotsaAuthMod was constructed (for the setup() warning)
private:
  void _loopModules(IotsaBaseModule *skip);
  IotsaBaseModule *_loopingModule = nullptr;  // whose loop() is running now
  bool _inAboutToBlock = false;
  static void _runPosted();
};

//
// Mix-in for the small set of modules that are single-instance infrastructure
// rather than application features: the HTTP/CoAP/HPS transports, IotsaConfigMod,
// IotsaRunmodeMod, IotsaBLEServerMod, IotsaWifiMod and IotsaOtaMod (cwi-dis/iotsa#85). It gives every one of them the same
// "there is at most one, create it on demand" shape, replacing the hand-rolled
// static-pointer + ensureServiceMod() copies these classes used to carry.
//
//  - instance() returns the one instance, or nullptr if the sketch never
//    declared it and nothing has called ensure() yet.
//  - ensure(app, ...) returns it, constructing it the first time via
//    T(IotsaApplication&, <extra args forwarded>) -- most modules take just
//    (app), IotsaConfigMod also takes an auth provider. Call this from code that
//    needs the module to exist whether or not the sketch declared it -- e.g.
//    IotsaBleApiService::setup() needs a BLE server module for HPS to work, see
//    cwi-dis/iotsa#84.
//  - T's real constructor must call claimSingleton(this), so an
//    explicitly-declared instance is registered too and a second one is caught
//    (loud log, first instance kept) rather than silently shadowing the first.
//
template <class T>
class IotsaSingletonModule {
public:
  static T *instance() { return _instance; }
  template <typename... Args>
  static T *ensure(IotsaApplication &app, Args&&... args) {
    // constructor calls claimSingleton()
    if (_instance == nullptr) new T(app, static_cast<Args&&>(args)...);
    return _instance;
  }
protected:
  static void claimSingleton(T *self) {
    if (_instance != nullptr && _instance != self) {
      IOTSA_LOG("iotsa", "duplicate singleton module ignored, keeping the first");
      return;
    }
    _instance = self;
  }
  static T *_instance;
};

template <class T> T *IotsaSingletonModule<T>::_instance = nullptr;

class IotsaAuthMod;

class IotsaContext;

// An auth provider's answer (cwi-dis/iotsa#296).
enum class IotsaAccess {
  Granted,
  NeedsCredentials,   // unknown caller: logging in might help
  Forbidden           // known caller without the right: logging in won't help
};

class IotsaAuthenticationProvider {
public:
  IotsaAuthenticationProvider() {}
  IotsaAuthenticationProvider(const IotsaAuthenticationProvider& that) = delete;
  IotsaAuthenticationProvider& operator=(const IotsaAuthenticationProvider& that) = delete;

  virtual ~IotsaAuthenticationProvider() {}
  // May the caller of this request do what needs this right? Never asked
  // about IOTSA_RIGHT_NONE or IOTSA_RIGHT_NEVER: the request context answers
  // those itself. Only answers: the request context sends the challenge or
  // the refusal. The credentials come
  // from the context (ctx.hasBasicAuth(), ctx.bearerToken()).
  virtual IotsaAccess allows(IotsaContext& ctx, const char *right) = 0;
};

class IotsaApiProvider;
//
// One API request, the same over REST, CoAP and HPS (cwi-dis/iotsa#280,
// cwi-dis/iotsa#296): parse the body, call the handler with a request context,
// save when a Saved field changed, and build the reply: the JSON reply, or
// {"iotsa_api_error": "..."}. http: the request came over the HTTP server
// (REST), which carries credentials. Returns the status; responded: the reply
// was already sent (a 401 challenge), the transport sends nothing.
//
int iotsaApiRequest(IotsaApiProvider *provider, const char *transport, IotsaApiOperation verb, const char *path, const char *body, bool http, String& reply, bool& responded);
// A transport's own refusal (no such path, method not allowed), as the same
// kind of error reply. Returns status.
int iotsaApiRefusal(const char *transport, const char *method, const char *path, int status, const char *message, String& reply);
// The log lines for one request, the same for every transport (cwi-dis/iotsa#182):
// "iotsaApi: <transport>: <method> <path> ...". The request line is
// debug, written when the request arrives, with detail like "(57 bytes)"; the
// result line is always shown, written when the reply goes out, with the
// result like "ok, saved" or "404 not found".
void iotsaApiLogRequest(const char *transport, const char *method, const char *path, const String& detail);
void iotsaApiLogResult(const char *transport, const char *method, const char *path, const String& result);

//
// REST/CoAP/HPS API provider interface. Every module implements this (with
// harmless do-nothing defaults) whether or not it actually registers any
// endpoint with a transport -- see cwi-dis/iotsa#206.
//
class IotsaApiContext;
class IotsaBaseModule;

class IotsaApiProvider {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  // The API handlers, with a request context (cwi-dis/iotsa#296): read the
  // request with ctx.arg*(), write the reply with ctx.show(), refuse with
  // ctx.error(). The framework checks the rights, saves when a Saved field
  // changed, and reports unused fields. The defaults answer 405.
  virtual void getHandler(IotsaApiContext& ctx);
  virtual void putHandler(IotsaApiContext& ctx);
  virtual void postHandler(IotsaApiContext& ctx);
  // The module behind this provider, for the request context.
  virtual IotsaBaseModule *asModule() { return nullptr; }
  // Web page handler, invoked by IotsaApiServiceWeb for modules that opt in (see
  // cwi-dis/iotsa#213) -- structurally different from the JSON get/put/post handlers
  // above: no path argument (a module has at most one page), reads its own arguments
  // straight off the server, and does its own auth check internally.
  virtual void webHandler() {}
};

//
// Native BLE GATT provider interface (UUID-keyed, no JSON payload). Same
// always-present-with-defaults treatment as IotsaApiProvider -- see #206.
//
class IotsaBLEProvider {
public:
  typedef const char * UUIDstring;

  virtual ~IotsaBLEProvider() {}
  // Both run in the loop task (cwi-dis/iotsa#236), see IotsaBLECharacteristicCallbacks.
  virtual bool blePutHandler(UUIDstring charUUID) { return false; }
  virtual bool bleGetHandler(UUIDstring charUUID) { return false; }
  // A write whose blePutHandler() was skipped because loop() didn't get to it in
  // time. Called on the NimBLE host task: only latch something.
  virtual void blePutTimedOut(UUIDstring charUUID) {}
  // The name in the "iotsaApi: ble: ..." log lines (cwi-dis/iotsa#182), or
  // nullptr for no lines (HPS, whose requests have lines of their own).
  virtual const char *bleLogName() { return nullptr; }
};

// Base for every iotsa module -- deliberately lenient (setup()/loop() are the
// only pure virtuals; lateSetup()/info() have harmless no-op defaults
// rather than being forced) since some modules (e.g. IotsaCoapServiceMod, a
// lifecycle-only companion mod with no page/endpoint of its own) genuinely
// have nothing to contribute to either. A real module is expected to
// override both anyway -- an empty info() or a no-op lateSetup() would be
// an obviously incomplete module, not a subtle bug -- so this used to be two
// classes (one lenient, one forcing overrides via `= 0`) purely to catch
// that mistake at compile time; merged back into one, see cwi-dis/iotsa#206.
// The right everyone has, also without logging in: for things that are
// harmless or confirmed at the device (Runmode's identify). Handled by the
// request context, not by the auth providers (cwi-dis/iotsa#284).
#define IOTSA_RIGHT_NONE "none"
// The right nobody has: a value that can be written but never read back, such
// as a password (cwi-dis/iotsa#296). Handled by the request context.
#define IOTSA_RIGHT_NEVER "never"

class IotsaBaseModule : public IotsaApiProvider, public IotsaBLEProvider {
  friend class IotsaApplication;
  friend class IotsaContext;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaHttpServiceMod;
  friend class IotsaRunmodeMod;
public:
  const char *bleLogName() override { return name.c_str(); }
  IotsaBaseModule(IotsaApplication &_app, bool early=false)
  : app(_app),
    nextModule(NULL)
  {
    if (early) {
      app.addModEarly(this);
    } else {
      app.addMod(this);
    }
  }
  // Modules no longer take an auth provider: the application has one, see
  // IotsaApplication::setAuth() (cwi-dis/iotsa#284). Deleted rather than just
  // removed, so old code passing one fails to compile instead of the pointer
  // silently converting to `bool early`.
  IotsaBaseModule(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool early=false) = delete;
  IotsaBaseModule& operator=(const IotsaBaseModule& that) = delete;

  virtual void setup() = 0;
  virtual void loop() = 0;
  virtual void configLoad() {}
  virtual void configSave() {}
  // Unconditional (like getHandler/putHandler/postHandler/bleGetHandler/
  // blePutHandler) -- #206 decided info() should stay mandatory, "cheap,
  // always-present, no reason to make it optional" -- this was the one part of
  // that decision that hadn't actually been done yet, see cwi-dis/iotsa#205.
  virtual String info();
  // Also unconditional: pure string-escaping utilities, no actual dependency on
  // IOTSA_WITH_WEB -- used by web-server-extension modules (e.g. IotsaFilesMod's
  // directory listing) that need only IOTSA_HAS_WEBSERVER, not a web UI.
  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation
  static void percentDecode(const String &src, String &dst); // Helper - convert string from url-encoded to normal
  virtual void lateSetup();
  // Called once, after every module's setup() and lateSetup() have run. For most
  // modules the default no-op is correct; it exists for the small set of modules that
  // must not go "live" (e.g. start BLE advertising) until every other module has had a
  // chance to register with them during setup()/lateSetup() -- see cwi-dis/iotsa#113.
  virtual void lateSetupDone() {}
  IotsaBaseModule *asModule() override { return this; }
  virtual void sleepWakeupNotification(bool sleep) {}
  // Whether this module exposes a REST/CoAP/HPS API (overridden by IotsaModule).
  virtual bool hasApi() const { return false; }
  // The rights a request to this module starts with, see readsNeedRight().
  const char *readRight() const { return _readRight; }
  const char *changeRight() const { return _changeRight; }

protected:
  // Rights for the whole module, page and API alike: every read, or every
  // change, needs this right, unless a handler names another one with
  // ctx.showsNeedRight()/ctx.argsNeedRight(). The default for both is
  // IOTSA_RIGHT_NONE: open to everyone on the network
  // (https://github.com/cwi-dis/iotsa/issues/183#issuecomment-6368033789).
  // For a module whose content is private as a whole (RFID cards). Call
  // them in lateSetup(), next to api.setup().
  void readsNeedRight(const char *right) { _readRight = right; }
  void changesNeedRight(const char *right) { _changeRight = right; }
#ifdef IOTSA_HAS_WEBSERVER
  // A raw web handler (cwi-dis/iotsa#296): it answers the request itself (a
  // file, a tar, an upload), outside the page machinery. The framework checks
  // the right first, and answers a refusal itself (401 or 403);
  // IOTSA_RIGHT_NONE: no check. GET and POST, or GET only. Call it in lateSetup().
  typedef std::function<void()> WebHandler;
  void addWebHandler(const char *path, WebHandler handler, const char *right, bool getOnly = false);
  // A file upload (POST, multipart): upload is called for every part as it
  // arrives, done at the end, to answer. A refused upload writes nothing.
  void addWebHandler(const char *path, WebHandler done, WebHandler upload, const char *right);
  // Every path under prefix that no other handler takes (Files' /data/...).
  void addWebPrefixHandler(const char *prefix, WebHandler handler, const char *right);
  // Refused (and answered) for this right?
  bool _webRefused(const char *right);
#endif
  IotsaApplication &app;
  IotsaBaseModule *nextModule;
  String name;
private:
  const char *_readRight = IOTSA_RIGHT_NONE;
  const char *_changeRight = IOTSA_RIGHT_NONE;
};

class IotsaConfigFileLoad;
class IotsaConfigFileSave;

#ifdef IOTSA_WITH_WEB
class IotsaWebContext;
#endif

class IotsaModObject {
public:
  virtual ~IotsaModObject() {}
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) = 0;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) = 0;
#ifdef IOTSA_WITH_WEB
  // The web interface (cwi-dis/iotsa#183), the same shape as a module's.
  // Names are relative: whoever holds the object puts it in a Scope.
  // A field group implements args and fields, a table row implements row
  // (ctx.showCell(), once in header mode, then once per element).
  virtual void formHandler_args(IotsaWebContext& ctx) {}
  virtual void formHandler_fields(IotsaWebContext& ctx) {}
  virtual void formHandler_row(IotsaWebContext& ctx) {}
#endif
};

extern IotsaConfig iotsaConfig;
#endif
