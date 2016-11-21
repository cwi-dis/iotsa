#ifndef _IOTSA_H_
#define _IOTSA_H_

#include <ESP8266WebServer.h>

//
// Global defines, changes some behaviour in the whole library
//
#define IFDEBUG if(1)
#define CONFIGURATION_MODE_TIMEOUT  120  // How long to go to temp configuration mode at reboot
#define WITH_LED      // Define to turn on/off LED to show activity
#ifdef WITH_LED
const int led = 15;
#define LED if(1)
#else
const int led = 99;
#define LED if(0)
#endif


class IotsaMod;

class IotsaApplication {
friend class IotsaMod;
public:
  IotsaApplication(ESP8266WebServer &_server, const char *_title)
  : server(_server), 
    firstModule(NULL), 
    firstEarlyModule(NULL), 
    title(_title),
    haveOTA(false)
    {}
  void addMod(IotsaMod *mod);
  void addModEarly(IotsaMod *mod);
  void setup();
  void serverSetup();
  void loop();
  void enableOta() { haveOTA = true; }
  bool otaEnabled() { return haveOTA; }
protected:
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
  void webServerRootHandler();
  ESP8266WebServer &server;
  IotsaMod *firstModule;
  IotsaMod *firstEarlyModule;
  String title;
  bool haveOTA;
};

class IotsaAuthMod;

class IotsaMod {
friend class IotsaApplication;
public:
  IotsaMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false)
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
  virtual void serverSetup() = 0;
  virtual void loop() = 0;
  virtual String info() = 0;
protected:
  bool needsAuthentication();
  IotsaApplication &app;
  ESP8266WebServer &server;
  IotsaAuthMod *auth;
  IotsaMod *nextModule;
};

class IotsaAuthMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;	// Inherit constructor
  virtual bool needsAuthentication();
};

inline bool IotsaMod::needsAuthentication() { return auth ? auth->needsAuthentication() : false; }

extern bool configurationMode;        // True if we have no config, and go into AP mode
typedef enum { TMPC_NORMAL, TMPC_CONFIG, TMPC_OTA, TMPC_RESET } config_mode;
extern config_mode  tempConfigurationMode;    // Current configuration mode (i.e. after a power cycle)
extern unsigned long tempConfigurationModeTimeout;  // When we reboot out of current configuration mode
extern config_mode  nextConfigurationMode;    // Next configuration mode (i.e. before a power cycle)
extern unsigned long nextConfigurationModeTimeout;  // When we abort nextConfigurationMode and revert to normal operation
extern String hostName;

#endif
