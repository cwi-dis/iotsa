#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfig.h"

class IotsaWifiMod : public IotsaApiMod {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) 
  : IotsaApiMod(_app, _auth, true),
    configMod(_app, _auth)
  {}
	void setup();
	void serverSetup();
	void loop();
  String info();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
private:
  void configLoad();
  void configSave();
  void handler();

  String ssid;
  String ssidPassword;
  bool haveMDNS;
  IotsaConfigMod configMod;
};

#endif
