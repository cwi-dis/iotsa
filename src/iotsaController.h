#ifndef _IOTSACONTROLLER_H_
#define _IOTSACONTROLLER_H_
#include <stdint.h>
#include "iotsaConfig.h"   // iotsa_mode, extensionCallback

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
//
// Still to move here: radio-enablement and sleep/wake policy.
//

class IotsaController {
public:
  void begin();  // from IotsaApplication::setup(), right after configLoad()
  void tick();   // from IotsaApplication::loop()

  void requestReboot(uint32_t ms);   // ESP.restart() after ms milliseconds

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
  iotsa_mode _mode = IOTSA_MODE_NORMAL;
  iotsa_mode _nextMode = IOTSA_MODE_NORMAL;
  uint32_t _modeEndTime = 0;
  uint32_t _nextModeEndTime = 0;
  extensionCallback _extendCb;
};

extern IotsaController iotsaController;
#endif
