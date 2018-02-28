#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaWifiMod : public IotsaApiMod {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaApiMod(_app, _auth, true) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
  using IotsaBaseMod::needsAuthentication;
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
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
