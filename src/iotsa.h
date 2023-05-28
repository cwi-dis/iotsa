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

class IotsaBaseMod;
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
  friend class IotsaBaseMod;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaWebServerMixin;
  friend class IotsaBatteryMod;
public:
  IotsaApplication(const char *_title);
  // Explicitly disable copy constructor and assignment
  IotsaApplication(const IotsaApplication& that) = delete;
  IotsaApplication& operator=(const IotsaApplication& that) = delete;

  void addMod(IotsaBaseMod *mod);
  void addModEarly(IotsaBaseMod *mod);
  void setup();
  void serverSetup();
  void loop();
  IotsaStatusInterface *status;
protected:
  IotsaBaseMod *firstModule;
  IotsaBaseMod *firstEarlyModule;
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

class IotsaBaseMod {
  friend class IotsaApplication;
  friend class IotsaConfigMod;
  friend class IotsaWifiMod;
  friend class IotsaWebServerMixin;
  friend class IotsaBatteryMod;
public:
  IotsaBaseMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
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
  IotsaBaseMod& operator=(const IotsaBaseMod& that) = delete;
  
  virtual void setup() = 0;
  virtual void loop() = 0;
  virtual void configLoad() {}
  virtual void configSave() {}
#ifdef IOTSA_WITH_WEB
  virtual String info();
#endif
  virtual void serverSetup();
  virtual bool needsAuthentication(const char *right=NULL);
  virtual bool needsAuthentication(const char *obj, IotsaApiOperation verb);
  virtual void sleepWakeupNotification(bool sleep) {}

protected:
  IotsaApplication &app;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
#endif
  IotsaAuthenticationProvider *auth;
  IotsaBaseMod *nextModule;
  String name;
};

class IotsaMod : public IotsaBaseMod {
public:
  IotsaMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBaseMod(_app, _auth, early)
  {
  }
  IotsaMod& operator=(const IotsaMod& that) = delete;
  virtual void serverSetup() override = 0;

#ifdef IOTSA_WITH_WEB
  virtual String info() override = 0;
  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation
  static void percentDecode(const String &src, String &dst); // Helper - convert string from url-encoded to normal
#endif

protected:
};

class IotsaAuthMod : public IotsaMod, public IotsaAuthenticationProvider {
public:
  using IotsaMod::IotsaMod;	// Inherit constructor
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
