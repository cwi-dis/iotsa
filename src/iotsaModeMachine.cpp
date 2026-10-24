#include "iotsa.h"
#include "iotsaModeMachine.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"

// Extracted from IotsaController (cwi-dis/iotsa#106 step 5a). Behaviour unchanged
// -- IotsaController's methods are now thin forwarders to this object.

static const char *PENDING_MODE_FILE = "/config/pendingmode.cfg";

uint32_t IotsaModeMachine::_windowMillis() {
  return 1000UL * iotsaConfig.configurationModeTimeout;
}

void IotsaModeMachine::begin(bool hardwareReset) {
  // Consume the one-shot mailbox that requestMode() wrote before the last reboot.
  iotsa_mode pending = IOTSA_MODE_NORMAL;
  if (iotsaConfigFileExists(PENDING_MODE_FILE)) {
    int v;
    { IotsaConfigFileLoad cf(PENDING_MODE_FILE); cf.get("mode", v, (int)IOTSA_MODE_NORMAL); }
    pending = (iotsa_mode)v;
    IOTSA_FS.remove(PENDING_MODE_FILE);   // consumed exactly once
  }
  if (pending == IOTSA_MODE_NORMAL) return;

  // Anti-tamper: a requested mode is honoured only after a hardware reset -- never
  // a software reboot / watchdog / crash, or a bug/attacker could walk the device
  // into a privileged mode. The reset-reason check is iotsaStatus.wasHardwareReset().
  if (!hardwareReset) {
    IOTSA_LOG("iotsaController", "pending mode %d not honoured (not a hardware reset)", (int)pending);
    return;
  }

  _mode = pending;
  _modeDeadline.arm(_windowMillis());
  IOTSA_LOG("iotsaController", "entering mode %d, timeout in %u ms", (int)_mode, (unsigned)_windowMillis());
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaModeMachine::tick() {
  // Active mode timed out -> back to normal. Pending request timed out -> just
  // drop the request (+ its mailbox file); no mode was active to "end".
  if (_modeDeadline.fired()) endConfigurationMode();
  if (_nextModeDeadline.fired()) {
    IOTSA_LOG("iotsaController", "pending mode request timed out");
    _clearPending();
  }
}

void IotsaModeMachine::_clearPending() {
  _nextMode = IOTSA_MODE_NORMAL;
  _nextModeDeadline.disarm();
  if (iotsaConfigFileExists(PENDING_MODE_FILE)) IOTSA_FS.remove(PENDING_MODE_FILE);
}

void IotsaModeMachine::requestMode(iotsa_mode mode) {
  if (mode == IOTSA_MODE_NORMAL) {
    _clearPending();
    return;
  }
  _nextMode = mode;
  _nextModeDeadline.arm(_windowMillis());
  IotsaConfigFileSave cf(PENDING_MODE_FILE);
  cf.put("mode", (int)mode);
}

void IotsaModeMachine::allowRequestedConfigurationMode() {
  if (_nextMode == _mode) return;
  IOTSA_LOG("iotsaController", "switching to mode %d", (int)_nextMode);
  _mode = _nextMode;
  _modeDeadline.arm(_windowMillis());
  _clearPending();
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaModeMachine::endConfigurationMode() {
  IOTSA_LOG("iotsaController", "mode %d ended", (int)_mode);
  _mode = IOTSA_MODE_NORMAL;
  _modeDeadline.disarm();
  _clearPending();
}

void IotsaModeMachine::extendWindow() {
  if (_mode == IOTSA_MODE_NORMAL) return;   // no maintenance-mode window to extend
  IOTSA_LOG_DEBUG("iotsaController", "mode %d extended", (int)_mode);
  _modeDeadline.arm(_windowMillis());
}

const char *IotsaModeMachine::modeName(iotsa_mode mode) const {
  // Not gated on IOTSA_WITH_WEB: /api/status reports it too (cwi-dis/iotsa#176).
  if (mode == IOTSA_MODE_NORMAL) return "normal";
  if (mode == IOTSA_MODE_CONFIG) return "configuration";
  if (mode == IOTSA_MODE_OTA) return "OTA";
  if (mode == IOTSA_MODE_FACTORY_RESET) return "factory-reset";
  return "unknown";
}

void IotsaModeMachine::factoryReset() {
  IOTSA_LOG("iotsaController", "factory reset");
  iotsaBreadcrumbs.addBreadcrumb(IOTSA_CRUMB_FACTORY_RESET);
  iotsaController.pauseWatchdog();   // format() can take long, and we reboot anyway
  delay(1000);
  IOTSA_LOG("iotsaController", "formatting " IOTSA_FS_NAME);
  IOTSA_FS.format();
  IOTSA_LOG("iotsaController", "format done, rebooting");
  delay(2000);
  ESP.restart();
}
