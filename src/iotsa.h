#ifndef _IOTSA_H_
#define _IOTSA_H_

#ifdef ESP32
#include <ESP32WebServer.h>
typedef ESP32WebServer IotsaWebServer;
#else
#include <ESP8266WebServer.h>
typedef ESP8266WebServer IotsaWebServer;
#endif
//
// Global defines, changes some behaviour in the whole library
//
#define IFDEBUG if(1)
#define CONFIGURATION_MODE_TIMEOUT  120  // How long to go to temp configuration mode at reboot

// Magic to allow logging to be kept in-core, if wanted, by using
// IotsaSerial in stead of Serial.
extern Print *iotsaOverrideSerial;
#define IotsaSerial (*iotsaOverrideSerial)

class IotsaBaseMod;

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

class IotsaApplication {
friend class IotsaBaseMod;
public:
  IotsaApplication(IotsaWebServer &_server, const char *_title)
  : status(NULL),
    server(_server), 
    firstModule(NULL), 
    firstEarlyModule(NULL), 
    title(_title),
    haveOTA(false)
    {}
  void addMod(IotsaBaseMod *mod);
  void addModEarly(IotsaBaseMod *mod);
  void setup();
  void serverSetup();
  void loop();
  void enableOta() { haveOTA = true; }
  bool otaEnabled() { return haveOTA; }
  IotsaStatusInterface *status;
protected:
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
  void webServerRootHandler();
  IotsaWebServer &server;
  IotsaBaseMod *firstModule;
  IotsaBaseMod *firstEarlyModule;
  String title;
  bool haveOTA;
};

class IotsaAuthMod;

class IotsaBaseMod {
friend class IotsaApplication;
public:
  IotsaBaseMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false)
  : app(_app), 
  	server(_app.server), 
  	auth(_auth), 
  	nextModule(NULL)
  {
    if (early) {
      app.addModEarly(this);
    } else {
      app.addMod(this);
    }
  }
  virtual void setup() = 0;
  virtual void loop() = 0;
  virtual String info();
  virtual void serverSetup();
protected:
  bool needsAuthentication() { return needsAuthentication(NULL);}
  virtual bool needsAuthentication(const char *right);
//  virtual bool needsAuthentication(const char *right, IotsaApiOperation verb);
  IotsaApplication &app;
  IotsaWebServer &server;
  IotsaAuthMod *auth;
  IotsaBaseMod *nextModule;
};

class IotsaMod : public IotsaBaseMod {
public:
  IotsaMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false)
  : IotsaBaseMod(_app, _auth, early)
  {
  }
  virtual String info() = 0;
  virtual void serverSetup() = 0;

  static String htmlEncode(String data); // Helper - convert strings to HTML-safe representation

protected:
};

class IotsaAuthMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;	// Inherit constructor
  virtual bool needsAuthentication(const char *right);
  virtual bool needsAuthentication(const char *object, IotsaApiOperation verb);
};
inline bool IotsaBaseMod::needsAuthentication(const char *right) { return auth ? auth->needsAuthentication(right) : false; }
inline bool IotsaAuthMod::needsAuthentication(const char *object, IotsaApiOperation verb) { return auth ? auth->needsAuthentication(object, verb) : false; }

extern bool configurationMode;        // True if we have no config, and go into AP mode
typedef enum { TMPC_NORMAL, TMPC_CONFIG, TMPC_OTA, TMPC_RESET } config_mode;
extern config_mode  tempConfigurationMode;    // Current configuration mode (i.e. after a power cycle)
extern unsigned long tempConfigurationModeTimeout;  // When we reboot out of current configuration mode
extern config_mode  nextConfigurationMode;    // Next configuration mode (i.e. before a power cycle)
extern unsigned long nextConfigurationModeTimeout;  // When we abort nextConfigurationMode and revert to normal operation
extern String hostName;

#endif
