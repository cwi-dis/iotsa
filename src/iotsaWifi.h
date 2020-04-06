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
#ifdef ESP32
  void _wifiCallback(system_event_id_t event, system_event_info_t info);
#endif
  void _wifiGotoMode();
  bool _wifiStartStation();
  bool _wifiWaitStation();
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
  bool wantWifiModeSwitch;
  unsigned long searchTimeoutMillis;
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaWifiMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() {}
  void serverSetup() {}
  void loop() {}
  String info() {return "";}
};
#endif // IOTSA_WITH_WIFI || IOTSA_WITH_PLACEHOLDERS

#endif
