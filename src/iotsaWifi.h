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
  // Copy IotsaWifiController's published state into the iotsaConfig fields other
  // modules read; start/stop mDNS and poke the status LED on the edges.
  void _publishControllerState();

  IotsaWifiController _controller{*this};  // the policy layer (cwi-dis/iotsa#106)
  bool _lastStaConnected = false;
  bool _lastApActive = false;

  String ssid;
  String ssidPassword;
  bool wifiPowerReduction;

  // ===== Driver surface (cwi-dis/iotsa#106) =====
  // A thin, policy-free mechanism layer: imperative radio ops, events latched by
  // the platform WiFi callbacks, and introspection. Policy lives in
  // IotsaWifiController (below). The shared value types (IotsaWifiActualState /
  // IotsaWifiEvents / IotsaWifiStaFailReason) are declared in iotsaWifiController.h.
  // For now this coexists with the legacy _wifi* machinery and loop() logic, which
  // still runs; that goes once the controller is wired in (slice 3b).
public:
  // Radio ops: fire, report whether the *attempt* was issued (not whether it
  // completed). No timers, no policy, no writes to iotsaConfig.wifiMode.
  bool startStation(const String& targetSsid, const String& targetPsk, uint8_t channel = 0, const uint8_t* bssid = nullptr);
  void stopStation();
  bool startAP(const String& apName);
  void stopAP();
  void reinitStack();             // disconnect(true)/mode(OFF)/mode(STA) -- unwedge, never reboot

  IotsaWifiActualState readActualState() const;
  IotsaWifiEvents drainEvents();   // read-and-clear the latched events

private:
  void _installDriverEventHandlers();
  static IotsaWifiStaFailReason _reduceStaFailReason(int reason);
  // Latch storage, written only from the platform WiFi callbacks (foreign task
  // context) -- single-word writes only, see cwi-dis/iotsa#236.
  volatile bool _evStaGotIp = false;
  volatile bool _evStaFailed = false;
  volatile uint8_t _evStaFailReason = 0;
  volatile bool _evStaLost = false;
  volatile bool _evApClientCountChanged = false;
  volatile uint8_t _evLastChannel = 0;
  uint8_t _evLastBssid[6] = {0};
  bool _driverHaveIp = false;      // touched only in the callbacks: staLost vs staFailed
  bool _driverHandlersInstalled = false;
#ifndef ESP32
  WiFiEventHandler _evH_gotIp, _evH_disconnected, _evH_apConnect, _evH_apDisconnect;
#endif
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
