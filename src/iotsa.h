#ifndef _IOTSA_H_
#define _IOTSA_H_

#include "iotsaVersion.h"
#include "iotsaBuildOptions.h"
#include <Print.h>

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

//
// Global defines, changes some behaviour in the whole library
//
#ifdef IOTSA_WITH_DEBUG
#define IFDEBUG if(1)
#else
#define IFDEBUG if(0)
#endif

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
  // Set when an authentication check fails: the authenticator has then already
  // sent its own response (e.g. 401), so the REST transport must not send another
  // when the handler returns. Reset by the transport before each handler call.
  bool requestDenied = false;
  void setup();
  void lateSetup();
  void loop();
  // Call just before deliberately holding loop() for a long time (an OTA
  // transfer): runs one more pass over the modules' loop(), so e.g. status
  // renderers show the latest state, which then stays visible while loop() is
  // blocked (cwi-dis/iotsa#259). The module whose loop() is calling this is
  // skipped. Does nothing when nested, or outside the loop task.
  void aboutToBlock();
#ifdef IOTSA_HAS_WEBSERVER
  // Convenience for app-level sketch code (e.g. tests/KitchenSink, examples/Hello,
  // examples/Log) that registers its own raw handler outside of any module method,
  // and for any module that isn't itself an API-having module with a webHandler()
  // (web-server-extension modules like IotsaFilesUploadMod/IotsaLoggerMod/
  // IotsaSimpleMod, and a module's own registrations that fall outside its page,
  // like IotsaConfigMod's cert upload, cwi-dis/iotsa#221) -- forwards to the shared
  // IotsaHttpServiceMod, set once in the constructor.
  // Also still used by the auth-provider modules (IotsaUserMod/IotsaMultiUserMod/
  // IotsaCapabilityMod), whose allows() implementations need to read credentials
  // off the live HTTP request regardless of which module they're authenticating for
  // -- a known wart, see cwi-dis/iotsa#107 (rights-earning redesign), which will
  // replace this with a proper per-request context instead.
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
#ifdef ESP32
  TaskHandle_t _loopTask = nullptr;
#endif
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
      IotsaSerial.println("IOTSA: duplicate singleton module ignored, keeping the first");
      return;
    }
    _instance = self;
  }
  static T *_instance;
};

template <class T> T *IotsaSingletonModule<T>::_instance = nullptr;

class IotsaAuthMod;

class IotsaAuthenticationProvider {
public:
  IotsaAuthenticationProvider() {}
  IotsaAuthenticationProvider(const IotsaAuthenticationProvider& that) = delete;
  IotsaAuthenticationProvider& operator=(const IotsaAuthenticationProvider& that) = delete;

  virtual ~IotsaAuthenticationProvider() {}
  virtual bool allows(const char *right=NULL) = 0;
  virtual bool allows(const char *obj, IotsaApiOperation verb) = 0;
};

// Fetch field `name` from an API request into `var`, if it has JSON type JT.
// The single implementation behind every getFromRequest() member (#261).
// A bool target accepts both a JSON bool and a JSON integer, whatever JT
// says, so both `true` and `1` work for every boolean field.
template <typename JT, typename CT> bool iotsaGetFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
  // Not via a JsonVariantConst: CT can be a mutable JsonArray/JsonObject.
  if (!reqObj[name].is<JT>()) return false;
  var = reqObj[name].as<CT>();
  return true;
}

template <typename JT> bool iotsaGetFromRequest(const JsonObject& reqObj, const char *name, bool& var) {
  JsonVariantConst v = reqObj[name];
  if (v.is<bool>()) {
    var = v.as<bool>();
    return true;
  }
  if (v.is<int>()) {
    var = (v.as<int>() != 0);
    return true;
  }
  return false;
}

//
// REST/CoAP/HPS API provider interface. Every module implements this (with
// harmless do-nothing defaults) whether or not it actually registers any
// endpoint with a transport -- see cwi-dis/iotsa#206.
//
class IotsaApiProvider {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  // Asked by the transports before calling a handler: true means "refuse"
  // (the auth provider has already sent its challenge, if any). Modules ask
  // the application's provider; override to exempt specific paths.
  virtual bool needsAuthentication(const char *obj, IotsaApiOperation verb) { return false; }
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  // Web page handler, invoked by IotsaApiServiceWeb for modules that opt in (see
  // cwi-dis/iotsa#213) -- structurally different from the JSON get/put/post handlers
  // above: no path argument (a module has at most one page), reads its own arguments
  // straight off the server, and does its own auth check internally.
  virtual void webHandler() {}
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    return iotsaGetFromRequest<JT>(reqObj, name, var);
  }
};

//
// Native BLE GATT provider interface (UUID-keyed, no JSON payload). Same
// always-present-with-defaults treatment as IotsaApiProvider -- see #206.
//
class IotsaBLEProvider {
public:
  typedef const char * UUIDstring;

  virtual ~IotsaBLEProvider() {}
  virtual bool blePutHandler(UUIDstring charUUID) { return false; }
  virtual bool bleGetHandler(UUIDstring charUUID) { return false; }
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
class IotsaBaseModule : public IotsaApiProvider, public IotsaBLEProvider {
  friend class IotsaApplication;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaHttpServiceMod;
  friend class IotsaRunmodeMod;
public:
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
  virtual bool needsAuthentication(const char *right=NULL);
  bool needsAuthentication(const char *obj, IotsaApiOperation verb) override;
  virtual void sleepWakeupNotification(bool sleep) {}
  // Whether this module exposes a REST/CoAP/HPS API (overridden by IotsaModule).
  virtual bool hasApi() const { return false; }

protected:
  IotsaApplication &app;
  IotsaBaseModule *nextModule;
  String name;
};

class IotsaAuthMod : public IotsaBaseModule, public IotsaAuthenticationProvider {
public:
  IotsaAuthMod(IotsaApplication &_app, bool early=false)
  : IotsaBaseModule(_app, early)
  {
    _app._haveAuthModule = true;
  }
  IotsaAuthMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool early=false) = delete;
};

class IotsaConfigFileLoad;
class IotsaConfigFileSave;

class IotsaModObject {
public:
  virtual ~IotsaModObject() {}
  virtual bool configLoad(IotsaConfigFileLoad& cf, const String& name) = 0;
  virtual void configSave(IotsaConfigFileSave& cf, const String& name) = 0;
#ifdef IOTSA_WITH_WEB
  // static virtual void formHandler_emptyfields(String& message) = 0;
  virtual void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) = 0;
  // static virtual void formHandler_TH(String& message, bool includeConfig) = 0;
  virtual void formHandler_TD(String& message, bool includeConfig) = 0;
  virtual bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) = 0;
#endif
};

extern IotsaConfig iotsaConfig;
#endif
