#include "iotsa.h"
#include "iotsaRunmode.h"   // pulls in iotsaBLEServer.h too
#ifdef IOTSA_HAS_SLEEP
#include "iotsaConfigFile.h"
#ifdef ESP32
#include <esp_wifi.h>
#include <esp_bt.h>
#if ESP_ARDUINO_VERSION_MAJOR > 2
#include "esp_system.h"
#include "rom/ets_sys.h"
#endif
// ESP32-S3 / -C3 have no separate RTC slow/fast memory power domains to turn off
// -- gate on the chip target directly (the bundled SDK predates the capability
// macros). Was IOTSA_BATTERY_CAN_RTC_MEM_POWER_DOMAINS, see cwi-dis/iotsa#205.
#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
#define IOTSA_SLEEP_CAN_RTC_MEM_POWER_DOMAINS 1
#endif
#endif // ESP32
#endif // IOTSA_HAS_SLEEP

// Mode / reboot / radio handlers are thin calls into iotsaController. Under
// IOTSA_HAS_SLEEP this module also owns the sleep executor + _sleepConfig
// (persisted to sleep.cfg); IotsaSleepPolicy::decide() borrows it (cwi-dis/iotsa#106).

#define SLEEP_DEBUG if(0)
// The hardware watchdog moved to IotsaController (cwi-dis/iotsa#106 step 5d) --
// it's a device-lifecycle concern, not sleep-coupled. _sleepTick() just brackets
// the sleep with iotsaController.pause/resumeWatchdog().

void IotsaRunmodeMod::setup() {
#ifdef IOTSA_HAS_SLEEP
  configLoad();
#ifdef ESP32
  iotsaController.sleep().didWakeFromSleep = (esp_sleep_get_wakeup_cause() != 0);
#endif
#endif // IOTSA_HAS_SLEEP
}

void IotsaRunmodeMod::lateSetup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  bleApi.addCharacteristic(currentModeUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x2700, "Current mode");
  bleApi.addCharacteristic(requestedModeUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Request mode for next boot");
  bleApi.addCharacteristic(rebootUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Reboot");
  bleApi.addCharacteristic(promoteModeUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Promote requested mode now");
  bleApi.addCharacteristic(wifiDisabledUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "WiFi radio disabled");
  bleApi.addCharacteristic(identifyUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Identify");
#endif
  api.setup("runmode", true, true);
  name = "runmode";
}

void IotsaRunmodeMod::loop() {
#ifdef IOTSA_WITH_BLE
  // Act on BLE writes here, out of the NimBLE host task (cwi-dis/iotsa#106).
  if (_pendingBleMode >= 0) {
    iotsaController.requestMode(iotsa_mode(_pendingBleMode));
    _pendingBleMode = -1;
  }
  if (_pendingBleReboot) {
    _pendingBleReboot = false;
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_BLE_MS);   // let the BLE stack finish the write ack (see #130)
  }
  if (_pendingBlePromoteMode) {
    _pendingBlePromoteMode = false;
    iotsaController.allowRequestedConfigurationMode();
  }
  if (_pendingBleWifiDisabled >= 0) {
    iotsaController.setWifiRadioEnabled(_pendingBleWifiDisabled == 0);
    _pendingBleWifiDisabled = -1;
  }
#endif
  if (_pendingIdentify) {
    _pendingIdentify = false;
    _doIdentify();
  }
#ifdef IOTSA_HAS_SLEEP
  _sleepTick();
#endif
}

void IotsaRunmodeMod::_doIdentify() {
  // cwi-dis/iotsa#133. Run from loop(), so an app handler that uses delay()-based
  // flashing doesn't block the BLE host task or the HTTP response.
  if (_identifyCallbacks.empty()) {
    IFDEBUG IotsaSerial.println("runmode: identify (no handler registered)");
    return;
  }
  for (auto& cb : _identifyCallbacks) cb();
}

bool IotsaRunmodeMod::_otaAvailable() const {
  for (IotsaBaseModule* m = app.firstEarlyModule; m != nullptr; m = m->nextModule)
    if (m->name == "ota") return true;
  for (IotsaBaseModule* m = app.firstModule; m != nullptr; m = m->nextModule)
    if (m->name == "ota") return true;
  return false;
}

#ifdef IOTSA_WITH_WEB
void IotsaRunmodeMod::webHandler() {
  String action;
  if (api.webService->server->hasArg("action")) {
    action = api.webService->server->arg("action");
  }
  String message = "<html><head><title>Iotsa runmode</title></head><body><h1>Iotsa runmode</h1>";

  if (action == "identify") {
    // No auth: identify is harmless and is used *before* you authenticate / OTA
    // (cwi-dis/iotsa#133).
    _pendingIdentify = true;
    message += "<p><em>Identifying.</em></p>";
  } else if (action != "") {
    if (needsAuthentication("config")) return;
    if (action == "setmode") {
      if (api.webService->server->hasArg("mode")) {
        String argValue = api.webService->server->arg("mode");
        if (argValue != "0") {
          iotsaController.requestMode(iotsa_mode(atoi(argValue.c_str())));
        }
      }
      if (api.webService->server->hasArg("factoryreset") && api.webService->server->hasArg("iamsure")
          && api.webService->server->arg("factoryreset") == "1" && api.webService->server->arg("iamsure") == "1") {
        iotsaController.requestMode(IOTSA_MODE_FACTORY_RESET);
      }
      if (iotsaController.requestedMode()) {
        message += "<p><em>Special mode ";
        message += iotsaController.modeName(iotsaController.requestedMode());
        message += " has been requested. Enable within ";
        message += String((iotsaController.requestedModeEndTime() - millis())/1000);
        message += " seconds by power cycling";
        if (iotsaController.rcmInteractionDescription()) {
          message += " or ";
          message += iotsaController.rcmInteractionDescription();
        }
        message += ".</em></p>";
      }
    } else if (action == "reboot") {
      iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
      message += "<p><em>Rebooting in 2 seconds.</em></p>";
    } else if (action == "wifi-disable") {
      iotsaController.setWifiRadioEnabled(false);
      message += "<p><em>WiFi radio disabled.</em></p>";
    } else if (action == "wifi-enable") {
      iotsaController.setWifiRadioEnabled(true);
      message += "<p><em>WiFi radio enabled.</em></p>";
#ifdef IOTSA_WITH_BLE
    } else if (action == "ble-disable") {
      iotsaController.setBleRadioEnabled(false);
      message += "<p><em>BLE radio disabled.</em></p>";
    } else if (action == "ble-enable") {
      iotsaController.setBleRadioEnabled(true);
      message += "<p><em>BLE radio enabled.</em></p>";
#endif
#ifdef IOTSA_HAS_SLEEP
    } else if (action == "save-sleep") {
      auto *srv = api.webService->server;
      IotsaSleepPolicy& sp = iotsaController.sleep();
      if (srv->hasArg("sleepMode")) _sleepConfig.mode = (IotsaSleepMode)srv->arg("sleepMode").toInt();
      if (srv->hasArg("sleepDuration")) _sleepConfig.sleepDuration = srv->arg("sleepDuration").toInt();
      if (srv->hasArg("wakeDuration")) _sleepConfig.wakeDuration = srv->arg("wakeDuration").toInt();
      if (srv->hasArg("bootExtraWakeDuration")) _sleepConfig.bootExtraWakeDuration = srv->arg("bootExtraWakeDuration").toInt();
      if (srv->hasArg("activityExtraWakeDuration")) sp.activityExtraWakeDuration = srv->arg("activityExtraWakeDuration").toInt();
      if (srv->hasArg("disableSleepOnWiFi")) _sleepConfig.disableSleepOnWiFi = srv->arg("disableSleepOnWiFi").toInt();
      if (srv->hasArg("disableWiFiOnSleep")) _sleepConfig.disableWiFiOnSleep = srv->arg("disableWiFiOnSleep").toInt();
      if (srv->hasArg("disableSleepOnUSBPower")) _sleepConfig.disableSleepOnUSBPower = srv->arg("disableSleepOnUSBPower").toInt();
#ifdef ESP32
      if (srv->hasArg("cpuFrequencyBoot")) _cpuFrequencyBoot = srv->arg("cpuFrequencyBoot").toInt();
      if (srv->hasArg("cpuFrequencySleep")) _cpuFrequencySleep = srv->arg("cpuFrequencySleep").toInt();
      if (srv->hasArg("cpuFrequency") && srv->arg("cpuFrequency").toInt() != 0) {
        setCpuFrequencyMhz(srv->arg("cpuFrequency").toInt());
      }
#endif
      iotsaController.extendCurrentMode();
      configSave();
      message += "<p><em>Sleep settings saved.</em></p>";
#endif // IOTSA_HAS_SLEEP
    }
  }

  // Status.
  if (iotsaController.currentMode()) {
    message += "<p>Currently in mode <b>";
    message += iotsaController.modeName(iotsaController.currentMode());
    message += "</b>, times out in ";
    message += String((iotsaController.currentModeEndTime() - millis())/1000);
    message += " seconds.</p>";
  } else {
    message += "<p>Currently in normal mode.</p>";
  }
  message += "<p>WiFi radio: ";
  message += iotsaStatus.wifiEnabled ? "enabled" : "disabled";
  message += ". Configuration-mode timeout: ";
  message += String(iotsaController.modeTimeout());
  message += " seconds.</p>";

  // Request a maintenance mode for the next boot.
  message += "<form method='post'>";
  message += "<input name='mode' type='radio' value='0' checked> Normal mode after next reboot.<br>";
  message += "<input name='mode' type='radio' value='1'> Configuration mode after next reboot.<br>";
  if (_otaAvailable()) {
    message += "<input name='mode' type='radio' value='2'> Over-the-air update mode after next reboot.<br>";
  }
  message += "<br><input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. ";
  message += "<input name='iamsure' type='checkbox' value='1'> Yes, I am sure.<br>";
  message += "<input type='submit' name='action' value='setmode'></form>";

  // Immediate actions.
  message += "<form method='post'>";
  message += "<input type='submit' name='action' value='identify'> Identify (blink/flash) this device.<br>";
  message += "<input type='submit' name='action' value='reboot'> Reboot now.<br>";
  message += "<input type='submit' name='action' value='wifi-disable'> ";
  message += "<input type='submit' name='action' value='wifi-enable'> Disable / enable WiFi radio now.<br>";
#ifdef IOTSA_WITH_BLE
  message += "<input type='submit' name='action' value='ble-disable'> ";
  message += "<input type='submit' name='action' value='ble-enable'> Disable / enable BLE radio now.<br>";
#endif
  message += "</form>";

#ifdef IOTSA_HAS_SLEEP
  {
    IotsaSleepPolicy& sp = iotsaController.sleep();
    message += "<h2>Sleep / wake</h2><form method='post'>";
    message += "Sleep mode: <select name='sleepMode'>";
    static const char *sleepModeNames[] = {"None", "Delay", "Light sleep", "Deep sleep", "Hibernate"};
    for (int i = 0; i < _IOTSA_SLEEP_MAX; i++) {
      message += "<option value='" + String(i) + "'" + ((int)_sleepConfig.mode == i ? " selected" : "") + ">" + sleepModeNames[i] + "</option>";
    }
    message += "</select><br>";
    message += "Sleep duration (ms): <input name='sleepDuration' value='" + String(_sleepConfig.sleepDuration) + "'><br>";
    message += "Wake duration (ms): <input name='wakeDuration' value='" + String(_sleepConfig.wakeDuration) + "'><br>";
    message += "Extra wake after activity (ms): <input name='activityExtraWakeDuration' value='" + String(sp.activityExtraWakeDuration) + "'><br>";
    message += "Extra wake after poweron/reset (ms): <input name='bootExtraWakeDuration' value='" + String(_sleepConfig.bootExtraWakeDuration) + "'><br>";
    message += "<input type='radio' name='disableSleepOnWiFi' value='0'" + String(_sleepConfig.disableSleepOnWiFi ? "" : " checked") + ">Sleep independent of WiFi ";
    message += "<input type='radio' name='disableSleepOnWiFi' value='1'" + String(_sleepConfig.disableSleepOnWiFi ? " checked" : "") + ">Don't sleep while WiFi active<br>";
    message += "<input type='radio' name='disableWiFiOnSleep' value='0'" + String(_sleepConfig.disableWiFiOnSleep ? "" : " checked") + ">Keep WiFi state on sleep ";
    message += "<input type='radio' name='disableWiFiOnSleep' value='1'" + String(_sleepConfig.disableWiFiOnSleep ? " checked" : "") + ">Disable WiFi before sleep<br>";
    message += "<input type='radio' name='disableSleepOnUSBPower' value='0'" + String(_sleepConfig.disableSleepOnUSBPower ? "" : " checked") + ">Sleep on USB or battery ";
    message += "<input type='radio' name='disableSleepOnUSBPower' value='1'" + String(_sleepConfig.disableSleepOnUSBPower ? " checked" : "") + ">Don't sleep on USB power<br>";
#ifdef ESP32
    message += "CPU frequency on boot (MHz): <input name='cpuFrequencyBoot' value='" + String(_cpuFrequencyBoot) + "'><br>";
    message += "CPU frequency on first sleep (MHz): <input name='cpuFrequencySleep' value='" + String(_cpuFrequencySleep) + "'><br>";
    message += "Current CPU frequency (MHz): <input name='cpuFrequency' value='" + String(getCpuFrequencyMhz()) + "'><br>";
#endif
    message += "<input type='submit' name='action' value='save-sleep'></form>";
  }
#endif // IOTSA_HAS_SLEEP

  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_WEB
String IotsaRunmodeMod::info() {
  String message;
  if (iotsaController.currentMode()) {
    message += "<p>In configuration mode ";
    message += iotsaController.modeName(iotsaController.currentMode());
    message += ", will timeout in " + String((iotsaController.currentModeEndTime()-millis())/1000) + " seconds.</p>";
  } else if (iotsaController.requestedMode()) {
    message += "<p>Special mode ";
    message += iotsaController.modeName(iotsaController.requestedMode());
    message += " has been requested. Enable within ";
    message += String((iotsaController.requestedModeEndTime() - millis())/1000);
    message += " seconds by power cycling";
    if (iotsaController.rcmInteractionDescription()) {
      message += " or ";
      message += iotsaController.rcmInteractionDescription();
    }
    message += ".</p>";
  } else if (iotsaController.currentModeEndTime()) {
    message += "<p>Strange, no configuration mode but timeout is " + String(iotsaController.currentModeEndTime()-millis()) + "ms.</p>";
  }
  message += "<p>See <a href=\"/runmode\">/runmode</a> to reboot or change mode.";
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaRunmodeMod::getHandler(const char *path, JsonObject& reply) {
  reply["currentMode"] = int(iotsaController.currentMode());
  if (iotsaController.currentMode()) {
    reply["currentModeTimeout"] = (iotsaController.currentModeEndTime() - millis())/1000;
  }
  reply["requestedMode"] = int(iotsaController.requestedMode());
  if (iotsaController.requestedMode()) {
    reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
  }
  reply["modeTimeout"] = iotsaController.modeTimeout();   // read-only mirror; owned by /api/config
  reply["wifiDisabled"] = !iotsaStatus.wifiEnabled;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabled"] = !iotsaController.bleRadioWanted();
#endif
  reply["identifyAvailable"] = !_identifyCallbacks.empty();   // cwi-dis/iotsa#133
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  reply["sleepMode"] = (int)_sleepConfig.mode;
  reply["sleepDuration"] = _sleepConfig.sleepDuration;
  reply["wakeDuration"] = _sleepConfig.wakeDuration;
  reply["bootExtraWakeDuration"] = _sleepConfig.bootExtraWakeDuration;
  reply["activityExtraWakeDuration"] = sp.activityExtraWakeDuration;
  reply["postponeSleep"] = sp.millisUntilSleepAllowed();
  reply["disableSleepOnWiFi"] = _sleepConfig.disableSleepOnWiFi;
  reply["disableWiFiOnSleep"] = _sleepConfig.disableWiFiOnSleep;
  reply["disableSleepOnUSBPower"] = _sleepConfig.disableSleepOnUSBPower;
#ifdef ESP32
  reply["cpuFrequency"] = getCpuFrequencyMhz();
  reply["cpuFrequencyBoot"] = _cpuFrequencyBoot;
  reply["cpuFrequencySleep"] = _cpuFrequencySleep;
#endif
#endif // IOTSA_HAS_SLEEP
  return true;
}

bool IotsaRunmodeMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();

  bool wifiDisabled;
  if (getFromRequest<int>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);
    anyChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<int>(reqObj, "bleDisabled", bleDisabled)) {
    iotsaController.setBleRadioEnabled(!bleDisabled);
    anyChanged = true;
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106).
    iotsaController.requestMode(iotsa_mode(reqModeInt));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      reply["requestedMode"] = int(iotsaController.requestedMode());
      reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
      reply["needsReboot"] = true;
    }
    anyChanged = true;
  }
  if (reqObj["reboot"]) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    anyChanged = true;
  }
  if (reqObj["identify"]) {
    _pendingIdentify = true;   // cwi-dis/iotsa#133; acted on in loop(), no auth
    anyChanged = true;
  }
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  bool sleepChanged = false;
  int intValue;
  if (reqObj["postponeSleep"].is<int>()) {
    iotsaController.postponeSleep(reqObj["postponeSleep"].as<int>());
    anyChanged = true;
  }
  if (getFromRequest<int>(reqObj, "sleepMode", intValue))              { _sleepConfig.mode = (IotsaSleepMode)intValue; sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "sleepDuration", _sleepConfig.sleepDuration))  { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "wakeDuration", _sleepConfig.wakeDuration))    { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "activityExtraWakeDuration", sp.activityExtraWakeDuration)) { sleepChanged = true; }
  if (getFromRequest<bool>(reqObj, "disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi)) { sleepChanged = true; }
  if (getFromRequest<bool>(reqObj, "disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep)) { sleepChanged = true; }
  if (getFromRequest<bool>(reqObj, "disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower)) { sleepChanged = true; }
#ifdef ESP32
  if (getFromRequest<int>(reqObj, "cpuFrequencyBoot", _cpuFrequencyBoot)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "cpuFrequencySleep", _cpuFrequencySleep)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "cpuFrequency", intValue)) {
    setCpuFrequencyMhz(intValue);
    IFDEBUG IotsaSerial.printf("Set CPU frequency to %d MHz\n", intValue);
    anyChanged = true;
  }
#endif
  if (sleepChanged) { configSave(); anyChanged = true; }
#endif // IOTSA_HAS_SLEEP
  if (checkUnhandled(reqObj)) {
    IotsaSerial.println("Unhandled IotsaApi parameters for /api/runmode");
  }
  return anyChanged;
}

#ifdef IOTSA_WITH_BLE
void IotsaRunmodeMod::allowBLEModeSwitch() {
  _bleAllowModeSwitch = true;
  iotsaController.allowRCMDescription("write promoteMode on the BLE runmode service");
}

bool IotsaRunmodeMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == requestedModeUUID) {
    _pendingBleMode = bleApi.getAsInt(requestedModeUUID);
    IFDEBUG IotsaSerial.printf("runmode: BLE requested mode %d\n", _pendingBleMode);
    return true;
  }
  if (charUUID == rebootUUID) {
    if (bleApi.getAsInt(rebootUUID)) _pendingBleReboot = true;
    IFDEBUG IotsaSerial.println("runmode: BLE reboot requested");
    return true;
  }
  if (charUUID == promoteModeUUID) {
    if (bleApi.getAsInt(promoteModeUUID)) {
      if (_bleAllowModeSwitch) {
        _pendingBlePromoteMode = true;
        IFDEBUG IotsaSerial.println("runmode: BLE promote-mode requested");
      } else {
        IFDEBUG IotsaSerial.println("runmode: BLE promote-mode requested but not allowed");
      }
    }
    return true;
  }
  if (charUUID == wifiDisabledUUID) {
    _pendingBleWifiDisabled = bleApi.getAsInt(wifiDisabledUUID) ? 1 : 0;
    IFDEBUG IotsaSerial.printf("runmode: BLE wifiDisabled=%d\n", _pendingBleWifiDisabled);
    return true;
  }
  if (charUUID == identifyUUID) {
    if (bleApi.getAsInt(identifyUUID)) _pendingIdentify = true;
    return true;
  }
  return false;
}

bool IotsaRunmodeMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == currentModeUUID) {
    bleApi.set(currentModeUUID, (uint8_t)iotsaController.currentMode());
    return true;
  }
  if (charUUID == requestedModeUUID) {
    bleApi.set(requestedModeUUID, (uint8_t)iotsaController.requestedMode());
    return true;
  }
  if (charUUID == wifiDisabledUUID) {
    bleApi.set(wifiDisabledUUID, (uint8_t)(iotsaStatus.wifiEnabled ? 0 : 1));
    return true;
  }
  return false;
}
#endif // IOTSA_WITH_BLE

#ifdef IOTSA_HAS_SLEEP
// ---------------------------------------------------------------------------
// Sleep/wake -- config persistence + the executor (was IotsaBatteryMod,
// cwi-dis/iotsa#106). _sleepConfig is this module's; decide() lives on IotsaSleepPolicy.
// ---------------------------------------------------------------------------

void IotsaRunmodeMod::configLoad() {
  IotsaSleepPolicy& sp = iotsaController.sleep();
  IotsaConfigFileLoad cf("/config/sleep.cfg");
  int value;
  cf.get("sleepMode", value, 0);
  if (value < 0 || value >= _IOTSA_SLEEP_MAX) value = IOTSA_SLEEP_NONE;
  _sleepConfig.mode = (IotsaSleepMode)value;
  cf.get("sleepDuration", _sleepConfig.sleepDuration, 0);
  cf.get("wakeDuration", _sleepConfig.wakeDuration, 0);
  cf.get("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration, 0);
  cf.get("activityExtraWakeDuration", sp.activityExtraWakeDuration, 0);
  cf.get("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi, 0);
  cf.get("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep, 0);
  cf.get("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower, 0);
#ifdef ESP32
  cf.get("cpuFrequencyBoot", _cpuFrequencyBoot, 0);
  cf.get("cpuFrequencySleep", _cpuFrequencySleep, 0);
  if (_cpuFrequencyBoot > 0) {
    setCpuFrequencyMhz(_cpuFrequencyBoot);
    IFDEBUG IotsaSerial.printf("Set CPU frequency to %d MHz on boot\n", _cpuFrequencyBoot);
  }
#endif
  // (millisAtWakeup is left alone -- _sleepTick() seeds it on its first run via
  // noteAwake(). configLoad() runs once at setup() when it's already 0.)
}

void IotsaRunmodeMod::configSave() {
  IotsaSleepPolicy& sp = iotsaController.sleep();
  IotsaConfigFileSave cf("/config/sleep.cfg");
  cf.put("sleepMode", (int)_sleepConfig.mode);
  cf.put("sleepDuration", _sleepConfig.sleepDuration);
  cf.put("wakeDuration", _sleepConfig.wakeDuration);
  cf.put("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration);
  cf.put("activityExtraWakeDuration", sp.activityExtraWakeDuration);
  cf.put("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi);
  cf.put("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep);
  cf.put("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower);
#ifdef ESP32
  cf.put("cpuFrequencyBoot", _cpuFrequencyBoot);
  cf.put("cpuFrequencySleep", _cpuFrequencySleep);
#endif
  IFDEBUG IotsaSerial.println("Saved sleep.cfg");
}

void IotsaRunmodeMod::_notifySleepWakeup(bool sleep) {
  for (IotsaBaseModule* m = app.firstEarlyModule; m != nullptr; m = m->nextModule) m->sleepWakeupNotification(sleep);
  for (IotsaBaseModule* m = app.firstModule; m != nullptr; m = m->nextModule) m->sleepWakeupNotification(sleep);
}

void IotsaRunmodeMod::_sleepTick() {
  IotsaSleepPolicy& sp = iotsaController.sleep();
  if (sp.millisAtWakeup == 0) {
    sp.noteAwake();
    IFDEBUG IotsaSerial.printf("runmode: wakeup at %u", (unsigned)sp.millisAtWakeup);
#ifdef ESP32
    IFDEBUG IotsaSerial.printf(" reason %d", (int)esp_sleep_get_wakeup_cause());
#endif
    IFDEBUG IotsaSerial.println();
  }
  if (_sleepConfig.mode == IOTSA_SLEEP_NONE) return;
  if (_pinDisableSleep >= 0 && digitalRead(_pinDisableSleep) == LOW) {
    SLEEP_DEBUG IotsaSerial.printf("runmode: no sleep, pinDisableSleep=%d LOW\n", _pinDisableSleep);
    return;
  }
  if (!iotsaController.canSleep()) {
    SLEEP_DEBUG IotsaSerial.println("runmode: no sleep, canSleep() false");
    return;
  }
  IotsaSleepDecision d = sp.decide(_sleepConfig, iotsaStatus.onUsbPower);
  if (d.mode == IOTSA_SLEEP_NONE) return;

  // Committed to sleeping in some form.
#ifdef ESP32
  iotsaController.pauseWatchdog();
#endif
  if (_sleepConfig.disableWiFiOnSleep && iotsaStatus.wifiEnabled) {
    static bool haveDisabledWiFi = false;
    if (!haveDisabledWiFi) {
      IFDEBUG IotsaSerial.println("Will disable WiFi for sleep");
      haveDisabledWiFi = true;
      iotsaController.setWifiRadioEnabled(false);
      iotsaController.postponeSleep(IotsaSleepPolicy::WIFI_SHUTDOWN_GRACE_MS);
      return;
    }
  }
  IFDEBUG IotsaSerial.printf("Going to sleep at %u for %u mode %d\n", (unsigned)millis(), (unsigned)d.durationMs, (int)d.mode);
  _notifySleepWakeup(true);
#ifdef ESP32
  if (_cpuFrequencySleep != 0) {
    static bool haveSetSleepFreq = false;
    if (!haveSetSleepFreq) {
      IFDEBUG IotsaSerial.printf("Setting CPU frequency to %d MHz for sleep\n", _cpuFrequencySleep);
      haveSetSleepFreq = true;
      setCpuFrequencyMhz(_cpuFrequencySleep);
    }
  }
#endif

  if (d.mode == IOTSA_SLEEP_DELAY) {
    // Not really sleep, just a delay. We return here afterwards.
    delay(d.durationMs);
    sp.didWakeFromSleep = true;
    sp.millisAtWakeup = 0;   // re-arm the wake window next tick
#ifdef ESP32
    iotsaController.resumeWatchdog();
#endif
    _notifySleepWakeup(false);
    return;
  }

#ifdef ESP32
  IFDEBUG delay(5); // flush serial
  if (d.durationMs) esp_sleep_enable_timer_wakeup(d.durationMs * 1000LL);

  if (d.mode == IOTSA_SLEEP_LIGHT) {
    // Everything stays powered, just runs slowly; we return here after waking.
#ifdef IOTSA_WITH_BLE
    bool btActive = IotsaBLEServerMod::pauseServer();
#endif
    esp_light_sleep_start();
    sp.noteWokeFromSleep();
    IFDEBUG IotsaSerial.printf("light sleep wakeup at %u\n", (unsigned)sp.millisAtWakeup);
#ifdef IOTSA_WITH_BLE
    if (btActive) IotsaBLEServerMod::resumeServer(_sleepConfig.wakeDuration);
#endif
    iotsaController.resumeWatchdog();
    _notifySleepWakeup(false);
    return;
  }

  // Deep sleep / hibernate: turn the radios off first, then don't return.
  if (iotsaStatus.wifiEnabled) esp_wifi_stop();
  esp_bt_controller_disable();
  if (d.mode == IOTSA_SLEEP_HIBERNATE) {
#ifdef IOTSA_SLEEP_CAN_RTC_MEM_POWER_DOMAINS
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
#endif
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  }
  esp_deep_sleep_start();
  IotsaSerial.println("esp_deep_sleep_start() failed?");
#else
  // esp8266: only deep sleep, via a full reboot on wake.
  ESP.deepSleep(d.durationMs * 1000LL);
#endif
}
#endif // IOTSA_HAS_SLEEP
