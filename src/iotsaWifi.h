#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigMod.h"

#ifdef IOTSA_WITH_WIFI
#ifdef IOTSA_WITH_API
#define IotsaWifiModBaseMod IotsaApiMod
#else
#define IotsaWifiModBaseMod IotsaMod
#endif

class IotsaWifiMod : public IotsaWifiModBaseMod {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL);
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
  void _wifiGotoMode();
  bool _wifiStartStation();
  void _wifiStopStation();
  void _wifiStartStationSucceeded();
  void _wifiStartStationFailed();
  bool _wifiStartAP(iotsa_wifi_mode mode);
  void _wifiStopAP(iotsa_wifi_mode mode);
  bool _wifiStartMDNS();
  void _wifiOff();
  IotsaConfigMod configMod;
  String ssid;
  String ssidPassword;
  unsigned long searchTimeoutMillis;
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaWifiMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() override {}
  void serverSetup() override {}
  void loop() override {}
  String info() override {return "";}
};
#endif // IOTSA_WITH_WIFI || IOTSA_WITH_PLACEHOLDERS

#endif
