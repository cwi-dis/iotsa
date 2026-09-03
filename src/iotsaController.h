#ifndef _IOTSACONTROLLER_H_
#define _IOTSACONTROLLER_H_
#include <stdint.h>
#include "iotsaConfig.h"       // iotsa_mode, extensionCallback
#include "iotsaStatus.h"       // iotsaConfigSettingsWritable() reads iotsaStatus
#include "iotsaSleepPolicy.h"  // IotsaController::_sleep

// Intended to be included from iotsa.h

//
// IotsaController -- the device policy coordinator (cwi-dis/iotsa#106).
//
// A framework global (like iotsaConfig / iotsaStatus), begin()'d and tick()'d by
// IotsaApplication. It owns the interlocking device-level policies that were
// scattered across IotsaConfig and IotsaBatteryMod, and will drive
// IotsaWifiController and the BLE server as subordinates. See
// docs/controller-architecture.md.
//
// Owns so far:
//   - the deferred-reboot timer (moved from IotsaConfig::loop())
//   - the iotsa_mode state machine: current/requested mode, the auto-expiry
//     timeout, the boot-time anti-tamper gate, and the one-shot "pending mode"
//     mailbox that carries a request across a reboot.
//   - WiFi + BLE radio-enablement policy: the boot defaults (wifiDisabledOnBoot,
//     bleDisabledOnBoot), the runtime enable/disable requests, and the mode
//     forcing (CONFIG/OTA -> WiFi on; CONFIG -> BLE on).
//   - sleep-inhibit bookkeeping (_sleep, an IotsaSleepPolicy): pauseSleep() /
//     postponeSleep() / canSleep(), moved off IotsaConfig.
//
// Still to move here (cwi-dis/iotsa#106 step 3): the rest of the sleep/wake
// policy -- sleep config, wake-window timing, the decide() function -- out of
// IotsaBatteryMod into _sleep, with the esp_*_sleep_start() machinery landing in
// IotsaRunmodeMod under IOTSA_HAS_SLEEP.
//

class IotsaController {
public:
  void begin();  // from IotsaApplication::setup(), right after configLoad()
  void tick();   // from IotsaApplication::loop()

  // Deferred reboot: ESP.restart() after `ms`, long enough for the reply that
  // asked for it to flush first. Use the REBOOT_DELAY_* constants below rather
  // than a bare number.
  void requestReboot(uint32_t ms);
  static constexpr uint32_t REBOOT_DELAY_HTTP_MS = 2000;  // after an HTTP/web response
  static constexpr uint32_t REBOOT_DELAY_BLE_MS  = 1000;  // after a BLE write-ack (cwi-dis/iotsa#130)

  // WiFi radio-enablement policy (cwi-dis/iotsa#106). The runtime desired state
  // (_wifiRadioEnabled) is seeded in begin() from !wifiDisabledOnBoot, then moved
  // by setWifiRadioEnabled(): the wifiDisabled REST/BLE toggle, iotsaBattery
  // (sleep), the BLE "enable WiFi" command. wifiRadioWanted() is the answer
  // IotsaWifiMod feeds to IotsaWifiController every tick -- the runtime state,
  // except CONFIG / OTA mode always force the radio on so the device stays
  // reachable while it is being worked on.
  void setWifiRadioEnabled(bool on) { _wifiRadioEnabled = on; }
  bool wifiRadioWanted() const {
    if (_mode == IOTSA_MODE_CONFIG || _mode == IOTSA_MODE_OTA) return true;
    return _wifiRadioEnabled;
  }

#ifdef IOTSA_WITH_BLE
  // BLE radio-enablement policy, same shape as WiFi above (cwi-dis/iotsa#106).
  // _bleRadioEnabled is seeded in begin() from !bleDisabledOnBoot, then moved by
  // setBleRadioEnabled() (the bleDisabled REST/web toggle). bleRadioWanted() is
  // what IotsaBLEServerMod reconciles its advertising with each tick. CONFIG mode
  // forces it on (a BLE gesture is how you steer a wifiDisabledOnBoot device into
  // a mode); OTA does NOT -- OTA is a WiFi path, BLE up would just waste radio.
  void setBleRadioEnabled(bool on) { _bleRadioEnabled = on; }
  bool bleRadioWanted() const {
    if (_mode == IOTSA_MODE_CONFIG) return true;
    return _bleRadioEnabled;
  }
#endif

  // ---- sleep/wake policy (cwi-dis/iotsa#106) ----
  // Sleep-inhibit surface, moved off IotsaConfig. Thin delegates to _sleep;
  // canSleep() additionally forces "no" while a maintenance mode is active (was
  // a separate inConfigurationMode() check in IotsaBatteryMod::loop()).
  void noteActivity() { _sleep.noteActivity(); }
  void pauseSleep()  { _sleep.pauseSleep(); }
  void resumeSleep() { _sleep.resumeSleep(); }
  void postponeSleep(uint32_t ms) { _sleep.postponeSleep(ms); }
  bool canSleep() {
    if (_mode == IOTSA_MODE_CONFIG || _mode == IOTSA_MODE_OTA) return false;
    return _sleep.canSleep();
  }
  IotsaSleepPolicy& sleep() { return _sleep; }

  // ---- iotsa_mode state machine ----

  // Request a mode for the *next* boot. Writes the mailbox; honoured by begin()
  // only after a hardware reset. IOTSA_MODE_NORMAL cancels a pending request.
  void requestMode(iotsa_mode mode);
  // Promote a pending request to active *now*, no reboot -- a gesture handler has
  // proved local physical presence. (Was IotsaConfig::allowRequestedConfigurationMode.)
  void allowRequestedConfigurationMode();
  // Back to IOTSA_MODE_NORMAL, and clear any pending request (RAM + mailbox file).
  void endConfigurationMode();
  // Push the auto-expiry of the current mode forward; also counts as activity for
  // the sleep wake-window (cwi-dis/iotsa#106). A no-op on the mode window when not
  // in a maintenance mode (the activity note still happens).
  void extendCurrentMode();
  void allowRCMDescription(const char *desc) { rcmInteractionDescription = desc; }
  void factoryReset();   // format the FS and reboot

  bool inConfigurationMode(bool extend = false);
  const char *modeName(iotsa_mode mode);

  iotsa_mode currentMode() const { return _mode; }
  iotsa_mode requestedMode() const { return _nextMode; }
  uint32_t currentModeEndTime() const { return _modeEndTime; }
  uint32_t requestedModeEndTime() const { return _nextModeEndTime; }

  // Auto-expiry of a maintenance mode, in seconds. Persisted by IotsaConfigMod
  // (config.cfg "rebootTimeout" key); was iotsaConfig.configurationModeTimeout
  // (cwi-dis/iotsa#106). Seeded from CONFIGURATION_MODE_TIMEOUT.
  uint32_t modeTimeout() const { return _modeTimeout; }
  void setModeTimeout(uint32_t seconds) { _modeTimeout = seconds; }

  // Human-readable hint for how to enter the requested mode on this device
  // ("press button 4 times", ...). Set by the app via allowRCMDescription().
  const char *rcmInteractionDescription = nullptr;

private:
  void _clearPendingMode();   // drop a pending request: RAM state + the mailbox file

  uint32_t _rebootAtMillis = 0;
  uint32_t _modeTimeout = CONFIGURATION_MODE_TIMEOUT;
  bool _wifiRadioEnabled = true;   // runtime desired state; begin() seeds it from !wifiDisabledOnBoot
#ifdef IOTSA_WITH_BLE
  bool _bleRadioEnabled = true;    // ditto, seeded from !bleDisabledOnBoot
#endif
  iotsa_mode _mode = IOTSA_MODE_NORMAL;
  iotsa_mode _nextMode = IOTSA_MODE_NORMAL;
  uint32_t _modeEndTime = 0;
  uint32_t _nextModeEndTime = 0;
  IotsaSleepPolicy _sleep;
};

extern IotsaController iotsaController;

// cwi-dis/iotsa#106 transitional: settings (WiFi credentials, hostname, ...) are
// writable in configuration mode, or on a not-yet-configured device serving its
// own AP. Replaces IotsaConfig::inConfigurationOrFactoryMode(); collapses to
// iotsaController.inConfigurationMode() once "no SSID => config mode" lands.
inline bool iotsaConfigSettingsWritable() {
  return iotsaController.inConfigurationMode()
      || (!iotsaStatus.wifiConfigured && iotsaStatus.wifiApActive);
}
#endif
