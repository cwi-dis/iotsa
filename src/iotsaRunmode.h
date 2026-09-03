#ifndef _IOTSARUNMODE_H_
#define _IOTSARUNMODE_H_
#include <functional>
#include <vector>
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
// Under IOTSA_HAS_SLEEP it also carries the sleep/wake executor: this module owns
// _sleepConfig (persisted to /config/sleep.cfg) + the CPU-frequency knobs + the
// esp_*_sleep_start() machinery; IotsaSleepPolicy::decide() borrows the config.
// The hardware watchdog moved to IotsaController (step 5d). Was IotsaBatteryMod
// (cwi-dis/iotsa#106).
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

  // "Identify" -- make this device physically announce itself (blink an LED,
  // beep, flash the output, ...) so a human can pick it out among several
  // similar devices, before doing something risky like an OTA. Registered by
  // app/module code; every registered handler is invoked from loop() when an
  // identify command arrives over REST / web / BLE (so an LED module and a
  // buzzer module can each register independently). Add-only, like module
  // registration. No handler => the command is accepted but nothing visible
  // happens. Scaffolding for cwi-dis/iotsa#133.
  typedef std::function<void(void)> IdentifyCallback;
  void addIdentifyCallback(IdentifyCallback cb) { _identifyCallbacks.push_back(cb); }
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
  // Identify (cwi-dis/iotsa#133): _pendingIdentify is set by any transport,
  // _doIdentify() runs the registered handlers from loop().
  std::vector<IdentifyCallback> _identifyCallbacks;
  bool _pendingIdentify = false;
  void _doIdentify();

  // "Is OTA-update mode offerable on this device?" -- true iff an IotsaOtaMod is
  // in the module list (it names itself "ota"). Was iotsaConfig.otaEnabled,
  // cwi-dis/iotsa#106.
  bool _otaAvailable() const;
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
  static constexpr UUIDstring identifyUUID = "6E5D0007-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
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
  // The sleep executor, called every loop(). Feeds _sleepConfig to
  // iotsaController.sleep().decide() and performs the actual sleep. cwi-dis/iotsa#106.
  void _sleepTick();
  void _notifySleepWakeup(bool sleep);
  // The persisted sleep config -- this module owns it and persists it (sleep.cfg);
  // IotsaSleepPolicy only borrows it via decide() (cwi-dis/iotsa#106 step 5b).
  IotsaSleepConfig _sleepConfig;
  int _pinDisableSleep = -1;
#ifdef ESP32
  int _cpuFrequencyBoot = 0;
  int _cpuFrequencySleep = 0;
#endif
#endif // IOTSA_HAS_SLEEP
};

#endif
