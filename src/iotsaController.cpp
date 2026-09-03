#include "iotsa.h"
#include "iotsaController.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"
#ifdef ESP32
#include <rom/rtc.h>
#else
#include <user_interface.h>
#endif

//
// Global variable definition
//
IotsaController iotsaController;

static const char *PENDING_MODE_FILE = "/config/pendingmode.cfg";

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void IotsaController::begin() {
  // Seed the runtime radio state from the persisted boot policy. From here on
  // setWifiRadioEnabled() / setBleRadioEnabled() (REST/web toggles, battery
  // sleep) move it, and {wifi,ble}RadioWanted() layer the mode forcing on top.
  _wifiRadioEnabled = !iotsaConfig.wifiDisabledOnBoot;
#ifdef IOTSA_WITH_BLE
  _bleRadioEnabled = !iotsaConfig.bleDisabledOnBoot;
#endif

  // Consume the one-shot mailbox that requestMode() wrote before the last reboot.
  iotsa_mode pending = IOTSA_MODE_NORMAL;
  if (iotsaConfigFileExists(PENDING_MODE_FILE)) {
    int v;
    { IotsaConfigFileLoad cf(PENDING_MODE_FILE); cf.get("mode", v, (int)IOTSA_MODE_NORMAL); }
    pending = (iotsa_mode)v;
    IOTSA_FS.remove(PENDING_MODE_FILE);   // consumed exactly once
  }
  if (pending == IOTSA_MODE_NORMAL) return;

  // Anti-tamper: a requested mode is honoured only after a hardware reset (the
  // reset button or a power cycle), never a software reboot / watchdog / crash --
  // otherwise a software bug or a remote attacker could walk the device into a
  // privileged mode.
#ifndef ESP32
  rst_info *rip = ESP.getResetInfoPtr();
  int reason = (int)rip->reason;
  bool hardwareReset = rip->reason == REASON_DEFAULT_RST || rip->reason == REASON_EXT_SYS_RST;
#else
  int reason = rtc_get_reset_reason(0);
  // xxxjack Not sure why I sometimes see the WDT reset on pressing the reset button...
  bool hardwareReset = reason == POWERON_RESET || reason == RTCWDT_RTC_RESET;
#endif
  if (!hardwareReset) {
    IFDEBUG IotsaSerial.printf("iotsaController: pending mode %d not honoured, reset reason %d\n", (int)pending, reason);
    return;
  }

  _mode = pending;
  _modeEndTime = millis() + 1000UL * _modeTimeout;
  IFDEBUG IotsaSerial.printf("iotsaController: entering mode %d, timeout at %u\n", (int)_mode, (unsigned)_modeEndTime);
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaController::tick() {
  if (_rebootAtMillis && millis() > _rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
  // Active mode timed out -> back to normal. Pending request timed out -> just
  // drop the request (+ its mailbox file); no mode was active to "end".
  if (_modeEndTime && millis() > _modeEndTime) endConfigurationMode();
  if (_nextModeEndTime && millis() > _nextModeEndTime) {
    IFDEBUG IotsaSerial.println("Pending mode request timed out");
    _clearPendingMode();
  }
}

void IotsaController::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  _rebootAtMillis = millis() + ms;
}

// ---------------------------------------------------------------------------
// iotsa_mode state machine
// ---------------------------------------------------------------------------

void IotsaController::_clearPendingMode() {
  _nextMode = IOTSA_MODE_NORMAL;
  _nextModeEndTime = 0;
  if (iotsaConfigFileExists(PENDING_MODE_FILE)) IOTSA_FS.remove(PENDING_MODE_FILE);
}

void IotsaController::requestMode(iotsa_mode mode) {
  if (mode == IOTSA_MODE_NORMAL) {
    _clearPendingMode();
    return;
  }
  _nextMode = mode;
  _nextModeEndTime = millis() + 1000UL * _modeTimeout;
  IotsaConfigFileSave cf(PENDING_MODE_FILE);
  cf.put("mode", (int)mode);
}

void IotsaController::allowRequestedConfigurationMode() {
  if (_nextMode == _mode) return;
  IFDEBUG IotsaSerial.print("Switching configurationMode to ");
  IFDEBUG IotsaSerial.println(_nextMode);
  _mode = _nextMode;
  _modeEndTime = millis() + 1000UL * _modeTimeout;
  _clearPendingMode();
  if (_mode == IOTSA_MODE_FACTORY_RESET) factoryReset();
}

void IotsaController::endConfigurationMode() {
  IFDEBUG IotsaSerial.println("Configuration mode ended");
  _mode = IOTSA_MODE_NORMAL;
  _modeEndTime = 0;
  _clearPendingMode();
}

void IotsaController::extendCurrentMode() {
  // Activity happened, so push the sleep wake-window out (was a callback into
  // IotsaBatteryMod, cwi-dis/iotsa#106).
  _sleep.noteActivity();
#ifndef ESP32
  ESP.wdtFeed();
#endif
  if (_mode == IOTSA_MODE_NORMAL) return;   // no maintenance-mode window to extend
  IFDEBUG IotsaSerial.println("Configuration mode extended");
  _modeEndTime = millis() + 1000UL * _modeTimeout;
}

bool IotsaController::inConfigurationMode(bool extend) {
  bool ok = _mode == IOTSA_MODE_CONFIG;
  if (ok && extend) extendCurrentMode();
  return ok;
}

const char *IotsaController::modeName(iotsa_mode mode) {
#ifdef IOTSA_WITH_WEB
  if (mode == IOTSA_MODE_NORMAL) return "normal";
  if (mode == IOTSA_MODE_CONFIG) return "configuration";
  if (mode == IOTSA_MODE_OTA) return "OTA";
  if (mode == IOTSA_MODE_FACTORY_RESET) return "factory-reset";
#endif // IOTSA_WITH_WEB
  return "unknown";
}

void IotsaController::factoryReset() {
  IFDEBUG IotsaSerial.println("configurationMode: Factory-reset");
  delay(1000);
  IFDEBUG IotsaSerial.println("Formatting " IOTSA_FS_NAME "...");
  IOTSA_FS.format();
  IFDEBUG IotsaSerial.println("Format done, rebooting.");
  delay(2000);
  ESP.restart();
}
