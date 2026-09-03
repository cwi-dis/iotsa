#ifndef _IOTSAMODEMACHINE_H_
#define _IOTSAMODEMACHINE_H_
#include <stdint.h>
#include "iotsaConfig.h"   // iotsa_mode

// Intended to be included from iotsaController.h

//
// IotsaModeMachine -- the iotsa_mode state machine (cwi-dis/iotsa#106): current /
// requested mode, the auto-expiry window, the boot-time anti-tamper gate, and the
// one-shot /config/pendingmode.cfg mailbox that carries a request across a
// reboot. Held by value as IotsaController::_modes; IotsaController forwards the
// public vocabulary (iotsaController.currentMode() etc.) to it unchanged.
//
// The auto-expiry duration is iotsaConfig.configurationModeTimeout (a persisted,
// user-edited knob -- it stays an iotsaConfig field, this class just reads it).
//
class IotsaModeMachine {
public:
  // Consume the mailbox + run the anti-tamper gate. hardwareReset: was this boot
  // a power-cycle / reset-button press, not a software reboot / watchdog / crash?
  void begin(bool hardwareReset);
  void tick();   // auto-expiry of the active / pending window

  void requestMode(iotsa_mode mode);          // for the *next* boot; NORMAL cancels
  void allowRequestedConfigurationMode();     // promote the pending request now
  void endConfigurationMode();                // back to NORMAL, clear pending
  void extendWindow();                        // push the active window's expiry out
  bool inConfigurationMode() const { return _mode == IOTSA_MODE_CONFIG; }
  const char *modeName(iotsa_mode mode) const;
  void factoryReset();                        // format the FS + reboot

  iotsa_mode currentMode() const { return _mode; }
  iotsa_mode requestedMode() const { return _nextMode; }
  uint32_t currentModeEndTime() const { return _modeEndTime; }
  uint32_t requestedModeEndTime() const { return _nextModeEndTime; }

  // Mode effects, one place (cwi-dis/iotsa#106 smell E3). CONFIG/OTA keep WiFi up
  // (device reachable while worked on) and forbid sleep; CONFIG also keeps BLE up
  // (a BLE gesture is how you steer a wifiDisabledOnBoot device), OTA does not.
  bool forcesWifiOn() const { return _mode == IOTSA_MODE_CONFIG || _mode == IOTSA_MODE_OTA; }
  bool forcesBleOn()  const { return _mode == IOTSA_MODE_CONFIG; }
  bool forbidsSleep() const { return _mode == IOTSA_MODE_CONFIG || _mode == IOTSA_MODE_OTA; }

  void allowRCMDescription(const char *desc) { _rcmDescription = desc; }
  const char *rcmInteractionDescription() const { return _rcmDescription; }

private:
  void _clearPending();          // pending RAM state + the mailbox file
  static uint32_t _windowMillis();   // 1000 * iotsaConfig.configurationModeTimeout

  iotsa_mode _mode = IOTSA_MODE_NORMAL;
  iotsa_mode _nextMode = IOTSA_MODE_NORMAL;
  uint32_t _modeEndTime = 0;
  uint32_t _nextModeEndTime = 0;
  const char *_rcmDescription = nullptr;
};
#endif
