#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "Wapp.h"

class WappWifiMod : public WappMod {
public:
  WappWifiMod(Wapplication &_app) : WappMod(_app, true) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void configLoad();
  void configSave();
  void handler();
  void handlerConfigmode();

  String ssid;
  String ssidPassword;
  bool haveMDNS;

};

#endif
