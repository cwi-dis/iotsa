#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaConfigMod : public IotsaApiMod {
public:
  IotsaConfigMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaApiMod(_app, _auth, true) {}
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
  void handlerNormalMode();
  void handlerConfigMode();

  String ssid;
  String ssidPassword;
  bool haveMDNS;

};

#endif
