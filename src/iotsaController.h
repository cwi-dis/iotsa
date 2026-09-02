#ifndef _IOTSACONTROLLER_H_
#define _IOTSACONTROLLER_H_
#include <stdint.h>
#include "iotsaConfig.h"   // iotsa_mode, extensionCallback
#include "iotsaStatus.h"   // iotsaConfigSettingsWritable() reads iotsaStatus

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
//   - WiFi radio-enablement policy: the boot default (wifiDisabledOnBoot), the
//     runtime enable/disable requests, and "CONFIG/OTA mode forces the radio on".
//
// Still to move here: BLE radio-enablement and sleep/wake policy.
//

class IotsaController {
public:
  void begin();  // from IotsaApplication::setup(), right after configLoad()
  void tick();   // from IotsaApplication::loop()

  void requestReboot(uint32_t ms);   // ESP.restart() after ms milliseconds

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

  // ---- iotsa_mode state machine ----

  // Request a mode for the *next* boot. Writes the mailbox; honoured by begin()
  // only after a hardware reset. IOTSA_MODE_NORMAL cancels a pending request.
  void requestMode(iotsa_mode mode);
  // Promote a pending request to active *now*, no reboot -- a gesture handler has
  // proved local physical presence. (Was IotsaConfig::allowRequestedConfigurationMode.)
  void allowRequestedConfigurationMode();
  // Enter config mode directly (WiFi module, factory-AP -> having-a-network).
  void beginConfigurationMode();
  // Back to IOTSA_MODE_NORMAL. In-RAM only; the mailbox is never touched here.
  void endConfigurationMode();
  // Push the auto-expiry of the current mode forward, and poke the extension callback.
  void extendCurrentMode();
  void setExtensionCallback(extensionCallback cb);
  void allowRCMDescription(const char *desc) { rcmInteractionDescription = desc; }
  void factoryReset();   // format the FS and reboot

  bool inConfigurationMode(bool extend = false);
  const char *modeName(iotsa_mode mode);

  iotsa_mode currentMode() const { return _mode; }
  iotsa_mode requestedMode() const { return _nextMode; }
  uint32_t currentModeEndTime() const { return _modeEndTime; }
  uint32_t requestedModeEndTime() const { return _nextModeEndTime; }

  // Human-readable hint for how to enter the requested mode on this device
  // ("press button 4 times", ...). Set by the app via allowRCMDescription().
  const char *rcmInteractionDescription = nullptr;

private:
  void _consumePendingMode();   // read + delete the mailbox file

  uint32_t _rebootAtMillis = 0;
  bool _wifiRadioEnabled = true;   // runtime desired state; begin() seeds it from !wifiDisabledOnBoot
#ifdef IOTSA_WITH_BLE
  bool _bleRadioEnabled = true;    // ditto, seeded from !bleDisabledOnBoot
#endif
  iotsa_mode _mode = IOTSA_MODE_NORMAL;
  iotsa_mode _nextMode = IOTSA_MODE_NORMAL;
  uint32_t _modeEndTime = 0;
  uint32_t _nextModeEndTime = 0;
  extensionCallback _extendCb;
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
