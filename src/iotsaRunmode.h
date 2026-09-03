#ifndef _IOTSARUNMODE_H_
#define _IOTSARUNMODE_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

//
// IotsaRunmodeMod -- the external control surface onto IotsaController
// (cwi-dis/iotsa#106, docs/controller-architecture.md).
//
// Core-tier: unconditionally ensure()d by IotsaApplication::setup(), the same
// treatment as IotsaConfigMod -- never an optional add. It is the one place,
// across every transport (REST/web/BLE), that a client steers the device's
// *operating state*: request a maintenance mode for the next boot, reboot,
// toggle the WiFi / BLE radios at runtime.
//
// Every handler here is thin glue: a call into iotsaController. The mode /
// reboot / radio keys also still appear in /api/config as [[deprecated]]
// forwarders for one release, so existing scripts and the Python CLI keep
// working until they move to /api/runmode (see the "Transition strategy"
// section of docs/controller-architecture.md).
//
// Under IOTSA_HAS_SLEEP it also carries the sleep/wake executor: the config +
// decision live in IotsaController's IotsaSleepPolicy, this module owns the
// esp_*_sleep_start() machinery, the watchdog timer, the CPU-frequency knobs and
// the /config/sleep.cfg persistence. Was IotsaBatteryMod (cwi-dis/iotsa#106).
//
class IotsaRunmodeMod : public IotsaModule, public IotsaSingletonModule<IotsaRunmodeMod> {
public:
  IotsaRunmodeMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaModule(_app, _auth, true)   // early: mode/reboot control belongs up before the app modules
  {
    claimSingleton(this);
  }
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
#ifdef IOTSA_HAS_SLEEP
  // A pin that, held LOW, blocks sleep (was IotsaBatteryMod::setPinDisableSleep).
  void setPinDisableSleep(int pin) { _pinDisableSleep = pin; }
#endif
#ifdef IOTSA_WITH_BLE
  // Opt in to letting a BLE client promote the pending requested mode to active
  // *now* (via the promoteMode characteristic) -- a BLE connection is taken as
  // proof of physical presence. Was IotsaBatteryMod::allowBLEConfigModeSwitch()
  // (cwi-dis/iotsa#106); the "is BLE presence enough" call is left to #107.
  void allowBLEModeSwitch();
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
#ifdef IOTSA_HAS_SLEEP
  void configLoad() override;
  void configSave() override;
#endif
#ifdef IOTSA_WITH_BLE
  // The BLE control service: read the current mode, request a mode for the next
  // boot, reboot. A client that discovers an iotsa device over BLE also wants to
  // steer it (cwi-dis/iotsa#106, #233). Writes are stashed here and acted on
  // from loop() -- blePutHandler runs in the NimBLE host task.
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  IotsaBleApiService bleApi;
  int _pendingBleMode = -1;         // -1: nothing pending; else an iotsa_mode value
  bool _pendingBleReboot = false;
  bool _pendingBlePromoteMode = false;
  int _pendingBleWifiDisabled = -1; // -1: nothing pending; 0: enable radio; 1: disable
  bool _bleAllowModeSwitch = false; // set by allowBLEModeSwitch()
  // Minted for iotsa#106 -- the iotsa runmode control service. xxxx0001 is the
  // service, xxxx0002+ the characteristics (same convention as elsewhere).
  static constexpr UUIDstring serviceUUID       = "6E5D0001-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring currentModeUUID   = "6E5D0002-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring requestedModeUUID = "6E5D0003-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring rebootUUID        = "6E5D0004-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring promoteModeUUID   = "6E5D0005-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring wifiDisabledUUID  = "6E5D0006-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
#endif // IOTSA_WITH_BLE
#ifdef IOTSA_HAS_SLEEP
private:
  // The sleep executor, called every loop(). Reads IotsaSleepPolicy for the
  // config + decision, performs the actual sleep. cwi-dis/iotsa#106.
  void _sleepTick();
  void _notifySleepWakeup(bool sleep);
  int _pinDisableSleep = -1;
#ifdef ESP32
  uint32_t _watchdogDuration = 0;
  int _cpuFrequencyBoot = 0;
  int _cpuFrequencySleep = 0;
#endif
#endif // IOTSA_HAS_SLEEP
};

#endif
