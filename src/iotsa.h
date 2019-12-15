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

#include "IotsaWebServer.h"

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
  friend class IotsaWebServerMixin;
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
  friend class IotsaWebServerMixin;
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
#ifdef IOTSA_WITH_WEB
  virtual String info();
#endif
  virtual void serverSetup();
  virtual bool needsAuthentication(const char *right=NULL);
  virtual bool needsAuthentication(const char *obj, IotsaApiOperation verb);

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
  virtual void serverSetup() = 0;

#ifdef IOTSA_WITH_WEB
  virtual String info() = 0;
  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation
  static void percentDecode(const String &src, String &dst); // Helper - convert string from url-encoded to normal
#endif

protected:
};

class IotsaAuthMod : public IotsaMod, public IotsaAuthenticationProvider {
public:
  using IotsaMod::IotsaMod;	// Inherit constructor
};

typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } config_mode;

class IotsaConfig {
public:
  bool configWasLoaded = false;
  bool wifiEnabled = false;
  bool otaEnabled = false;
  bool disableWifiOnBoot = false;
  bool wifiPrivateNetworkMode = false;
  config_mode configurationMode = IOTSA_MODE_NORMAL;
  unsigned long configurationModeEndTime = 0;
  config_mode nextConfigurationMode = IOTSA_MODE_NORMAL;
  unsigned long nextConfigurationModeEndTime = 0;
  String hostName = "";
  int configurationModeTimeout = 0;
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* httpsCertificate;
  size_t httpsCertificateLength;
  const uint8_t* httpsKey;
  size_t httpsKeyLength;
#endif // IOTSA_WITH_HTTPS
  uint32_t postponeSleepMillis = 0;
  int pauseSleepCount = 0;

  bool inConfigurationMode() { return configurationMode == IOTSA_MODE_CONFIG; }
  bool inConfigurationOrFactoryMode() { 
    if (configurationMode == IOTSA_MODE_CONFIG) return true;
    if (wifiPrivateNetworkMode && configurationModeEndTime == 0) return true;
    return false;
  }
  uint32_t getStatusColor() {
    if (wifiEnabled && !WiFi.isConnected()) return 0x3f1f00; // Orange: not connected to WiFi
    if (configurationMode == IOTSA_MODE_FACTORY_RESET) return 0x3f0000; // Red: Factory reset mode
    if (configurationMode == IOTSA_MODE_CONFIG) return 0x3f003f;	// Magenta: user-requested configuration mode
    if (configurationMode == IOTSA_MODE_OTA) return 0x003f3f;	// Cyan: OTA mode
    if (wifiPrivateNetworkMode) return 0x3f3f00; // Yellow: configuration mode (not user requested)
    return 0; // Off: all ok.
  }

  void pauseSleep() { pauseSleepCount++; }

  void resumeSleep() { pauseSleepCount--; }

  void postponeSleep(uint32_t ms) {
    uint32_t noSleepBefore = millis() + ms;
    if (noSleepBefore > postponeSleepMillis) postponeSleepMillis = noSleepBefore;
  }

  bool canSleep() {
    if (pauseSleepCount > 0) return false;
    if (millis() > postponeSleepMillis) postponeSleepMillis = 0;
    return postponeSleepMillis == 0;
  }
};

extern IotsaConfig iotsaConfig;
#endif
