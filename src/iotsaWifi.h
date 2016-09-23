#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"

class IotsaWifiMod : public IotsaMod {
public:
  IotsaWifiMod(IotsaApplication &_app) : IotsaMod(_app, true) {}
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
