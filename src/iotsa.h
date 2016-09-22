#ifndef _WAPP_H_
#define _WAPP_H_

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


class WappMod;

class Wapplication {
friend class WappMod;
public:
  Wapplication(ESP8266WebServer &_server, const char *_title)
  : server(_server), 
    firstModule(NULL), 
    firstEarlyModule(NULL), 
    title(_title) 
    {}
  void addMod(WappMod *mod);
  void addModEarly(WappMod *mod);
  void setup();
  void serverSetup();
  void loop();
protected:
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
  void webServerRootHandler();
  ESP8266WebServer &server;
  WappMod *firstModule;
  WappMod *firstEarlyModule;
  String title;
};

class WappMod {
friend class Wapplication;
public:
  WappMod(Wapplication &_app, bool early=false) : app(_app), server(_app.server), nextModule(NULL) {
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
  Wapplication &app;
  ESP8266WebServer &server;
  WappMod *nextModule;
};

extern bool configurationMode;        // True if we have no config, and go into AP mode
typedef enum { TMPC_NORMAL, TMPC_CONFIG, TMPC_OTA } config_mode;
extern config_mode  tempConfigurationMode;    // True if we go into AP mode for a limited time
extern unsigned long tempConfigurationModeTimeout;  // When we reboot out of temp configuration mode
extern String hostName;

#endif
