#ifndef _IOTSAWIFI_H_
#define _IOTSAWIFI_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigMod.h"
#include "iotsaWifiController.h"  // policy layer + the shared driver<->controller types

#ifdef IOTSA_WITH_WIFI
class IotsaWifiMod : public IotsaModule {
public:
  IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL);
	void setup() override;
	void lateSetup() override;
	void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
  bool _wifiStartMDNS();
  bool _wifiRadioWanted();   // !wifiDisabledOnBoot && iotsaController.wifiRadioEnabled()
  // Copy IotsaWifiController's published state into the iotsaConfig fields other
  // modules read; start/stop mDNS and poke the status LED on the edges.
  void _publishControllerState();

  // The mechanism/policy pair (cwi-dis/iotsa#106). IotsaWifiMod owns both, wires
  // them together, and keeps only the standard-module concerns for itself:
  // config load/save, the /api/wificonfig + web interface, info(), lifecycle.
  IotsaWifiDriver _driver;
  IotsaWifiController _controller{_driver};
  bool _lastStaConnected = false;
  bool _lastApActive = false;

  String ssid;
  String ssidPassword;
  bool wifiPowerReduction;
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaWifiMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
  String info() override {return "";}
};
#endif // IOTSA_WITH_WIFI || IOTSA_WITH_PLACEHOLDERS

#endif
