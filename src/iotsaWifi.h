#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"

class IotsaWifiMod : public IotsaMod {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaMod(_app, _auth, true) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void configLoad();
  void configSave();
  void handlerNormalMode();
  void handlerConfigMode();

  String ssid;
  String ssidPassword;
  bool haveMDNS;

};

#endif
