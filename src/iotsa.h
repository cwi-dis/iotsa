#ifndef _IOTSA_H_
#define _IOTSA_H_

#include "iotsaVersion.h"
#include "iotsaBuildOptions.h"
#include <Print.h>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "iotsaWebServer.h"
#include "iotsaConfig.h"
#include <ArduinoJson.h>

//
// Global defines, changes some behaviour in the whole library
//
#ifdef IOTSA_WITH_DEBUG
#define IFDEBUG if(1)
#else
#define IFDEBUG if(0)
#endif

#define CONFIGURATION_MODE_TIMEOUT 300  // How long to go to temp configuration mode at reboot

// Magic to allow logging to be kept in-core, if wanted, by using
// IotsaSerial in stead of Serial.
extern Print *iotsaOverrideSerial;
#define IotsaSerial (*iotsaOverrideSerial)

class IotsaBaseModule;
class IotsaConfigMod;

//
// Operations allowed via the API
//
typedef enum IotsaApiOperation {
  IOTSA_API_GET,
  IOTSA_API_PUT,
  IOTSA_API_POST,
  IOTSA_API_DELETE
} IotsaApiOperation;

//
// Status indication interface.
//
class IotsaStatusInterface {
public:
  virtual void showStatus() = 0;
};

class IotsaApplication : public IotsaWebServerMixin {
  friend class IotsaBaseModule;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaWebServerMixin;
  friend class IotsaBatteryMod;
public:
  IotsaApplication(const char *_title);
  // Explicitly disable copy constructor and assignment
  IotsaApplication(const IotsaApplication& that) = delete;
  IotsaApplication& operator=(const IotsaApplication& that) = delete;

  void addMod(IotsaBaseModule *mod);
  void addModEarly(IotsaBaseModule *mod);
  void setup();
  void serverSetup();
  void loop();
  IotsaStatusInterface *status;
protected:
  IotsaBaseModule *firstModule;
  IotsaBaseModule *firstEarlyModule;
  String title;
  bool haveOTA;
};

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

//
// REST/CoAP/HPS API provider interface. Every module implements this (with
// harmless do-nothing defaults) whether or not it actually registers any
// endpoint with a transport -- see cwi-dis/iotsa#206.
//
class IotsaApiProvider {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  // Web page handler, invoked by IotsaApiServiceWeb for modules that opt in (see
  // cwi-dis/iotsa#213) -- structurally different from the JSON get/put/post handlers
  // above: no path argument (a module has at most one page), reads its own arguments
  // straight off the server, and does its own auth check internally.
  virtual void webHandler() {}
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    if (reqObj[name].is<JT>()) {
      var = reqObj[name].as<CT>();
      return true;
    }
    return false;
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
// only pure virtuals; serverSetup()/info() have harmless no-op defaults
// rather than being forced) since some modules (e.g. IotsaCoapServiceMod, a
// lifecycle-only companion mod with no page/endpoint of its own) genuinely
// have nothing to contribute to either. A real module is expected to
// override both anyway -- an empty info() or a no-op serverSetup() would be
// an obviously incomplete module, not a subtle bug -- so this used to be two
// classes (one lenient, one forcing overrides via `= 0`) purely to catch
// that mistake at compile time; merged back into one, see cwi-dis/iotsa#206.
class IotsaBaseModule : public IotsaApiProvider, public IotsaBLEProvider {
  friend class IotsaApplication;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaWebServerMixin;
  friend class IotsaBatteryMod;
public:
  IotsaBaseModule(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : app(_app),
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  	server(_app.server),
#endif
  	auth(_auth),
  	nextModule(NULL)
  {
    if (early) {
      app.addModEarly(this);
    } else {
      app.addMod(this);
    }
  }
  IotsaBaseModule& operator=(const IotsaBaseModule& that) = delete;

  virtual void setup() = 0;
  virtual void loop() = 0;
  virtual void configLoad() {}
  virtual void configSave() {}
#ifdef IOTSA_WITH_WEB
  virtual String info();
  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation
  static void percentDecode(const String &src, String &dst); // Helper - convert string from url-encoded to normal
#endif
  virtual void serverSetup();
  // Called once, after every module's setup() and serverSetup() have run. For most
  // modules the default no-op is correct; it exists for the small set of modules that
  // must not go "live" (e.g. start BLE advertising) until every other module has had a
  // chance to register with them during setup()/serverSetup() -- see cwi-dis/iotsa#113.
  virtual void lateSetupDone() {}
  virtual bool needsAuthentication(const char *right=NULL);
  virtual bool needsAuthentication(const char *obj, IotsaApiOperation verb);
  virtual void sleepWakeupNotification(bool sleep) {}
  // Whether this module exposes a REST/CoAP/HPS API (overridden by IotsaModule).
  virtual bool hasApi() const { return false; }

protected:
  IotsaApplication &app;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
#endif
  IotsaAuthenticationProvider *auth;
  IotsaBaseModule *nextModule;
  String name;
};

class IotsaAuthMod : public IotsaBaseModule, public IotsaAuthenticationProvider {
public:
  using IotsaBaseModule::IotsaBaseModule;	// Inherit constructor
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
