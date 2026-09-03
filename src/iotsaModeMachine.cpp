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
    IFDEBUG IotsaSerial.printf("iotsaModeMachine: pending mode %d not honoured (not a hardware reset)\n", (int)pending);
    return;
  }

  _mode = pending;
  _modeEndTime = millis() + _windowMillis();
  IFDEBUG IotsaSerial.printf("iotsaModeMachine: entering mode %d, timeout at %u\n", (int)_mode, (unsigned)_modeEndTime);
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaModeMachine::tick() {
  // Active mode timed out -> back to normal. Pending request timed out -> just
  // drop the request (+ its mailbox file); no mode was active to "end".
  if (_modeEndTime && millis() > _modeEndTime) endConfigurationMode();
  if (_nextModeEndTime && millis() > _nextModeEndTime) {
    IFDEBUG IotsaSerial.println("Pending mode request timed out");
    _clearPending();
  }
}

void IotsaModeMachine::_clearPending() {
  _nextMode = IOTSA_MODE_NORMAL;
  _nextModeEndTime = 0;
  if (iotsaConfigFileExists(PENDING_MODE_FILE)) IOTSA_FS.remove(PENDING_MODE_FILE);
}

void IotsaModeMachine::requestMode(iotsa_mode mode) {
  if (mode == IOTSA_MODE_NORMAL) {
    _clearPending();
    return;
  }
  _nextMode = mode;
  _nextModeEndTime = millis() + _windowMillis();
  IotsaConfigFileSave cf(PENDING_MODE_FILE);
  cf.put("mode", (int)mode);
}

void IotsaModeMachine::allowRequestedConfigurationMode() {
  if (_nextMode == _mode) return;
  IFDEBUG IotsaSerial.print("Switching configurationMode to ");
  IFDEBUG IotsaSerial.println(_nextMode);
  _mode = _nextMode;
  _modeEndTime = millis() + _windowMillis();
  _clearPending();
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaModeMachine::endConfigurationMode() {
  IFDEBUG IotsaSerial.println("Configuration mode ended");
  _mode = IOTSA_MODE_NORMAL;
  _modeEndTime = 0;
  _clearPending();
}

void IotsaModeMachine::extendWindow() {
  if (_mode == IOTSA_MODE_NORMAL) return;   // no maintenance-mode window to extend
  IFDEBUG IotsaSerial.println("Configuration mode extended");
  _modeEndTime = millis() + _windowMillis();
}

const char *IotsaModeMachine::modeName(iotsa_mode mode) const {
#ifdef IOTSA_WITH_WEB
  if (mode == IOTSA_MODE_NORMAL) return "normal";
  if (mode == IOTSA_MODE_CONFIG) return "configuration";
  if (mode == IOTSA_MODE_OTA) return "OTA";
  if (mode == IOTSA_MODE_FACTORY_RESET) return "factory-reset";
#endif // IOTSA_WITH_WEB
  return "unknown";
}

void IotsaModeMachine::factoryReset() {
  IFDEBUG IotsaSerial.println("configurationMode: Factory-reset");
  delay(1000);
  IFDEBUG IotsaSerial.println("Formatting " IOTSA_FS_NAME "...");
  IOTSA_FS.format();
  IFDEBUG IotsaSerial.println("Format done, rebooting.");
  delay(2000);
  ESP.restart();
}
