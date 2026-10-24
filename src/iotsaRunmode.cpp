#include "iotsa.h"
#include "iotsaRunmode.h"   // pulls in iotsaBLEServer.h too
#include "iotsaFS.h"        // iotsaFS{Total,Used}Bytes() for /api/status
#include "iotsaConfigFile.h"
#ifdef IOTSA_HAS_SLEEP
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
// ESP32-C3 has no RTC peripherals power domain either (ESP-IDF 5 doesn't even
// declare ESP_PD_DOMAIN_RTC_PERIPH for it), see cwi-dis/iotsa#200.
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#define IOTSA_SLEEP_CAN_RTC_PERIPH_POWER_DOMAIN 1
#endif
#endif // ESP32
#endif // IOTSA_HAS_SLEEP

// Mode / reboot / radio handlers are thin calls into iotsaController. Under
// IOTSA_HAS_SLEEP this module also owns the sleep executor + _sleepConfig
// (persisted to sleep.cfg); IotsaSleepPolicy::decide() borrows it (cwi-dis/iotsa#106).

// The watchdog lives in IotsaController (cwi-dis/iotsa#106 step 5d, #244) --
// it's a device-lifecycle concern, not sleep-coupled. _sleepTick() just brackets
// the sleep with iotsaController.pause/resumeWatchdog().

void IotsaRunmodeMod::setup() {
  configLoad();
#ifdef IOTSA_HAS_SLEEP
#ifdef ESP32
  iotsaController.sleep().didWakeFromSleep = (esp_sleep_get_wakeup_cause() != 0);
#endif
#endif // IOTSA_HAS_SLEEP
#ifdef IOTSA_WITH_BLE
  // Deliberately here, in setup(), not lateSetup() below: IotsaRunmodeMod is
  // an early module, and IotsaApplication::setup() runs every early module's
  // setup() before *any* module's lateSetup() -- early or normal. The BLE
  // advertisement/scan-response payload has limited space
  // (NimBLEAdvertising::addServiceUUID() silently drops whatever doesn't
  // fit), so whichever service registers first wins the slot. Since every
  // other iotsa BLE service (an app's own, via IotsaBleApiService::setup())
  // is normally registered from that app's own setup() -- same convention
  // DimmerBLEServer already uses everywhere in lissabon -- runmode's own
  // registration has to happen in the *early* setup() phase to reliably win
  // that race and stay the one universally-discoverable "this is an iotsa
  // device" signal, confirmed missing from a real device's advertisement
  // live (2026-09-27) when it was still down in lateSetup().
  bleApi.setup(IotsaRunmodeBLE::serviceUUID, this);
  bleApi.addCharacteristic(IotsaRunmodeBLE::currentModeUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x2700, "Current mode");
  bleApi.addCharacteristic(IotsaRunmodeBLE::requestedModeUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Request mode for next boot");
  bleApi.addCharacteristic(IotsaRunmodeBLE::rebootUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Reboot");
  bleApi.addCharacteristic(IotsaRunmodeBLE::promoteModeUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Promote requested mode now");
  bleApi.addCharacteristic(IotsaRunmodeBLE::wifiDisabledUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "WiFi radio disabled");
  bleApi.addCharacteristic(IotsaRunmodeBLE::identifyUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Identify");
#endif
}

void IotsaRunmodeMod::lateSetup() {
  api.setup("runmode", true, true);
  api.setup("status", true, false, false, false);   // GET only, no web page (cwi-dis/iotsa#106 5e)
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
    IOTSA_LOG_DEBUG("iotsaRunmode", "identify (no handler registered)");
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
// The names are the /api/runmode field names, so the page and REST read alike.
// Auth as in putHandler(): identify and the mode request are open (harmless, or
// confirmed at the device), the rest needs the "config" right (cwi-dis/iotsa#284).

static const char * const requestableModeNames[] = {"Normal", "Configuration", "Over-the-air update"};
#ifdef IOTSA_HAS_SLEEP
static const char * const sleepModeNames[] = {"None", "Delay", "Light sleep", "Deep sleep", "Hibernate"};
#endif

void IotsaRunmodeMod::formHandler_args(IotsaWebContext& ctx) {
  // The protected fields first, then the open ones: a refusal comes before
  // anything happened, so the browser's retry after a login repeats nothing.
  // The actions come last, after ctx.failed() (cwi-dis/iotsa#296).
  ctx.argsNeedRight("config");
  bool reboot = ctx.argAction("reboot", IotsaWebContext::Transient);
  bool wifiDisabled = !iotsaStatus.wifiEnabled;
  bool wifiChanged = ctx.arg("wifiDisabled", wifiDisabled, IotsaWebContext::Transient);
#ifdef IOTSA_WITH_BLE
  bool bleDisabled = !iotsaController.bleRadioWanted();
  bool bleChanged = ctx.arg("bleDisabled", bleDisabled, IotsaWebContext::Transient);
#endif
  ctx.argCheckbox("debugLog", iotsaLogDebugEnabled, IotsaWebContext::Saved);
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  int sleepMode = int(_sleepConfig.mode);
  if (ctx.argChoice("sleepMode", sleepMode, _IOTSA_SLEEP_MAX, IotsaWebContext::Saved)) {
    _sleepConfig.mode = IotsaSleepMode(sleepMode);
  }
  ctx.arg("sleepDuration", _sleepConfig.sleepDuration, IotsaWebContext::Saved);
  ctx.arg("wakeDuration", _sleepConfig.wakeDuration, IotsaWebContext::Saved);
  ctx.arg("activityExtraWakeDuration", sp.activityExtraWakeDuration, IotsaWebContext::Saved);
  ctx.arg("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration, IotsaWebContext::Saved);
  ctx.argCheckbox("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi, IotsaWebContext::Saved);
  ctx.argCheckbox("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep, IotsaWebContext::Saved);
  ctx.argCheckbox("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower, IotsaWebContext::Saved);
#ifdef ESP32
  ctx.arg("cpuFrequencyBoot", _cpuFrequencyBoot, IotsaWebContext::Saved);
  ctx.arg("cpuFrequencySleep", _cpuFrequencySleep, IotsaWebContext::Saved);
  int cpuFrequency = getCpuFrequencyMhz();
  bool cpuChanged = ctx.arg("cpuFrequency", cpuFrequency, IotsaWebContext::Transient) && cpuFrequency != 0;
#endif
#endif // IOTSA_HAS_SLEEP

  ctx.argsNeedRight(IOTSA_RIGHT_NONE);
  bool identify = ctx.argAction("identify", IotsaWebContext::Transient);
  // Factory reset is not one of the choices: it has its own confirmed button.
  int requested = int(iotsaController.requestedMode());
  bool requestChanged = ctx.argChoice("requestedMode", requested, _otaAvailable() ? 3 : 2, IotsaWebContext::Transient);
  bool factoryReset = ctx.argConfirmedAction("factoryReset");

  if (ctx.failed()) return;
  if (reboot) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    ctx.feedback("Rebooting in 2 seconds.");
  }
  if (wifiChanged) iotsaController.setWifiRadioEnabled(!wifiDisabled);
#ifdef IOTSA_WITH_BLE
  if (bleChanged) iotsaController.setBleRadioEnabled(!bleDisabled);
#endif
#if defined(IOTSA_HAS_SLEEP) && defined(ESP32)
  if (cpuChanged) setCpuFrequencyMhz(cpuFrequency);
#endif
  if (identify) {
    _pendingIdentify = true;
    ctx.feedback("Identifying.");
  }
  if (requestChanged) iotsaController.requestMode(iotsa_mode(requested));
  if (factoryReset) iotsaController.requestMode(IOTSA_MODE_FACTORY_RESET);
}

void IotsaRunmodeMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    String current = iotsaController.modeName(iotsaController.currentMode());
    if (iotsaController.currentMode()) {
      current += ", times out in " + String(iotsaController.currentModeRemainingMillis()/1000) + " seconds";
    }
    ctx.showStatus("Current mode", "currentMode", current);
    if (iotsaController.requestedMode()) {
      String requested = iotsaController.modeName(iotsaController.requestedMode());
      requested += ", enable within " + String(iotsaController.requestedModeRemainingMillis()/1000) + " seconds by power cycling";
      String how = confirmDescription();
      if (how.length()) requested += " or " + how;
      ctx.showStatus("Requested mode", "requestedMode", requested);
    } else {
      ctx.showStatus("Requested mode", "requestedMode", "none");
    }
    ctx.showStatus("Configuration mode timeout", "", String(iotsaController.modeTimeout()) + " seconds");
    ctx.showStatus("WiFi radio", "wifiDisabled", iotsaStatus.wifiEnabled ? "enabled" : "disabled");
#ifdef IOTSA_WITH_BLE
    ctx.showStatus("BLE radio", "bleDisabled", iotsaController.bleRadioWanted() ? "enabled" : "disabled");
#endif

    // The same rights as in formHandler_args().
    ctx.argsNeedRight(IOTSA_RIGHT_NONE);
    ctx.showButton("identify", "Identify", IotsaWebContext::Transient);
    ctx.argsNeedRight("config");
    ctx.showButton("reboot", "Reboot now", IotsaWebContext::Transient);
    ctx.showButton("wifiDisabled", iotsaStatus.wifiEnabled ? "Disable WiFi radio" : "Enable WiFi radio", IotsaWebContext::Transient, iotsaStatus.wifiEnabled ? "1" : "0");
#ifdef IOTSA_WITH_BLE
    ctx.showButton("bleDisabled", iotsaController.bleRadioWanted() ? "Disable BLE radio" : "Enable BLE radio", IotsaWebContext::Transient, iotsaController.bleRadioWanted() ? "1" : "0");
#endif
    ctx.argsNeedRight(IOTSA_RIGHT_NONE);
    {
      IotsaWebContext::Form form(ctx, "Request", "Mode after the next reboot");
      int requested = int(iotsaController.requestedMode());
      ctx.showChoice("Mode", "requestedMode", requested, requestableModeNames, _otaAvailable() ? 3 : 2, IotsaWebContext::Transient);
    }
    {
      IotsaWebContext::Form form(ctx, "", "Factory reset");
      ctx.showConfirmedButton("factoryReset", "Factory reset and clear all files", IotsaWebContext::Transient);
    }
  } else {
    ctx.argsNeedRight("config");
    {
      IotsaWebContext::Form form(ctx, "Save", "Debug logging");
      ctx.showCheckbox("Debug logging", "debugLog", iotsaLogDebugEnabled, IotsaWebContext::Saved);
    }
#ifndef IOTSA_HAS_SLEEP
    ctx << "<h3>Sleep / wake</h3>\n<p>Not available: this firmware is built without sleep support.</p>\n";
#else
    IotsaSleepPolicy& sp = iotsaController.sleep();
    IotsaWebContext::Form form(ctx, "Save", "Sleep / wake");
    ctx.showChoice("Sleep mode", "sleepMode", int(_sleepConfig.mode), sleepModeNames, _IOTSA_SLEEP_MAX, IotsaWebContext::Saved);
    ctx.showField("Sleep duration", "sleepDuration", _sleepConfig.sleepDuration, IotsaWebContext::Saved, "ms");
    ctx.showField("Wake duration", "wakeDuration", _sleepConfig.wakeDuration, IotsaWebContext::Saved, "ms");
    ctx.showField("Extra wake after activity", "activityExtraWakeDuration", sp.activityExtraWakeDuration, IotsaWebContext::Saved, "ms");
    ctx.showField("Extra wake after power-on or reset", "bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration, IotsaWebContext::Saved, "ms");
    ctx.showCheckbox("Don't sleep while WiFi is active", "disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi, IotsaWebContext::Saved);
    ctx.showCheckbox("Disable WiFi before sleeping", "disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep, IotsaWebContext::Saved);
    ctx.showCheckbox("Don't sleep on USB power", "disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower, IotsaWebContext::Saved);
#ifdef ESP32
    ctx.showField("CPU frequency on boot", "cpuFrequencyBoot", _cpuFrequencyBoot, IotsaWebContext::Saved, "MHz, 0: unchanged");
    ctx.showField("CPU frequency on first sleep", "cpuFrequencySleep", _cpuFrequencySleep, IotsaWebContext::Saved, "MHz, 0: unchanged");
    ctx.showField("Current CPU frequency", "cpuFrequency", getCpuFrequencyMhz(), IotsaWebContext::Transient, "MHz");
#endif
#endif // IOTSA_HAS_SLEEP
  }
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_WEB
String IotsaRunmodeMod::info() {
  String message = "<p><b>Status:</b> " + htmlEncode(iotsaStatus.statusText()) + ".</p>";
  if (iotsaController.requestedMode()) {
    message += "<p>Enable within ";
    message += String(iotsaController.requestedModeRemainingMillis()/1000);
    message += " seconds by power cycling";
    String how = confirmDescription();
    if (how.length()) message += " or " + how;
    message += ".</p>";
  }
  message += "<p>See <a href=\"/runmode\">/runmode</a> to reboot or change mode.";
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(IotsaRunmodeBLE::serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

// Runmode's own settings: just the debug log switch (cwi-dis/iotsa#182), in
// runmode.cfg. The sleep settings keep their own sleep.cfg.
void IotsaRunmodeMod::configLoad() {
  IotsaConfigFileLoad cf("/config/runmode.cfg");
  int debugLog;
  cf.get("debugLog", debugLog, 1);
  iotsaLogDebugEnabled = debugLog != 0;
#ifdef IOTSA_HAS_SLEEP
  _sleepConfigLoad();
#endif
}

void IotsaRunmodeMod::configSave() {
  IotsaConfigFileSave cf("/config/runmode.cfg");
  cf.put("debugLog", (int)iotsaLogDebugEnabled);
#ifdef IOTSA_HAS_SLEEP
  _sleepConfigSave();
#endif
}

void IotsaRunmodeMod::getHandler(IotsaApiContext& ctx) {
  if (ctx.path() == "/api/status") {
    // Nested objects, nothing protected: written into the reply directly.
    JsonObject reply = ctx.reply();
    // Every-tick runtime observations -- the volatile counterpart of /api/config's
    // identity + persisted knobs (cwi-dis/iotsa#106 step 5e). Hosted here because
    // this is the runtime-control module and iotsaStatus has no module of its own.
    //
    // Grouped by subject (cwi-dis/iotsa#176). Semantic facts only: how the status
    // LED paints them (colour, rhythm) is presentation and deliberately not here.
    // (The control surface, /api/runmode, keeps its own flat legacy names.)
    JsonObject system = reply["system"].to<JsonObject>();
    system["uptime"] = millis() / 1000;
    system["bootCause"] = iotsaStatus.getBootReason();
    system["onUsbPower"] = iotsaStatus.onUsbPower;

    JsonObject mode = reply["mode"].to<JsonObject>();
    mode["current"] = int(iotsaController.currentMode());
    mode["currentName"] = iotsaController.modeName(iotsaController.currentMode());

    JsonObject wifi = reply["wifi"].to<JsonObject>();
    wifi["enabled"] = iotsaStatus.wifiEnabled;
    wifi["configured"] = iotsaStatus.wifiConfigured;
    wifi["stationConnected"] = iotsaStatus.wifiStationConnected;
    wifi["apActive"] = iotsaStatus.wifiApActive;
    wifi["apInUse"] = iotsaStatus.wifiApInUse;
    wifi["hunting"] = iotsaStatus.wifiHunting;
    wifi["private"] = iotsaStatus.wifiApActive && !iotsaStatus.wifiStationConnected;

    JsonObject fs = reply["fs"].to<JsonObject>();
    fs["totalBytes"] = iotsaFSTotalBytes();
    fs["usedBytes"] = iotsaFSUsedBytes();

    // Why the status indicator is currently overriding its normal display: an active
    // pulse ("Settings saved", "OTA update in progress") or a pending mode request.
    // null when there's nothing to say. Pulses are short (about 2s), so this means
    // "right now", not "recently". The reason is a static string, no lifetime issue.
    reply["notice"] = iotsaStatus.overrideSignal().reason;

    // What the device was doing before each earlier reset (cwi-dis/iotsa#276),
    // oldest first. uptime in seconds, at a resolution of about a minute.
    JsonObject crumbs = reply["breadcrumbs"].to<JsonObject>();
    crumbs["enabled"] = iotsaBreadcrumbsEnabled;
    JsonArray events = crumbs["events"].to<JsonArray>();
    uint32_t entries[IotsaBreadcrumbs::RING_SIZE];
    int count = iotsaBreadcrumbs.events(entries);
    for (int i = 0; i < count; i++) {
      uint8_t code = IotsaBreadcrumbs::entryCode(entries[i]);
      uint8_t arg = IotsaBreadcrumbs::entryArg(entries[i]);
      JsonObject event = events.add<JsonObject>();
      const char *name = IotsaBreadcrumbs::codeName(code);
      if (name) event["code"] = name; else event["code"] = code;
      event["arg"] = arg;
      event["uptime"] = IotsaBreadcrumbs::entryUptime(entries[i]);
      if (code == IOTSA_CRUMB_BOOT) {
        event["cause"] = IotsaStatus::bootReasonName(arg);
      } else if (code == IOTSA_CRUMB_LOOP) {
        // Module names are only right if the firmware hasn't changed since.
        uint8_t index = 0;
        IotsaBaseModule *m = app.firstEarlyModule;
        for (; m && index < arg; m = m->nextModule) index++;
        if (!m) {
          m = app.firstModule;
          for (; m && index < arg; m = m->nextModule) index++;
        }
        if (m) event["module"] = m->name;
      }
    }
    return;
  }
  ctx.show("currentMode", int(iotsaController.currentMode()));
  if (iotsaController.currentMode()) {
    ctx.show("currentModeTimeout", iotsaController.currentModeRemainingMillis()/1000);
  }
  ctx.show("requestedMode", int(iotsaController.requestedMode()));
  if (iotsaController.requestedMode()) {
    ctx.show("requestedModeTimeout", iotsaController.requestedModeRemainingMillis()/1000);
  }
  ctx.show("wifiDisabled", !iotsaStatus.wifiEnabled);
#ifdef IOTSA_WITH_BLE
  ctx.show("bleDisabled", !iotsaController.bleRadioWanted());
#endif
  ctx.show("identifyAvailable", !_identifyCallbacks.empty());   // cwi-dis/iotsa#133
  ctx.show("debugLog", iotsaLogDebugEnabled);   // cwi-dis/iotsa#182
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  ctx.show("sleepMode", (int)_sleepConfig.mode);
  ctx.show("sleepDuration", _sleepConfig.sleepDuration);
  ctx.show("wakeDuration", _sleepConfig.wakeDuration);
  ctx.show("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration);
  ctx.show("activityExtraWakeDuration", sp.activityExtraWakeDuration);
  ctx.show("postponeSleep", sp.millisUntilSleepAllowed());
  ctx.show("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi);
  ctx.show("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep);
  ctx.show("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower);
#ifdef ESP32
  ctx.show("cpuFrequency", getCpuFrequencyMhz());
  ctx.show("cpuFrequencyBoot", _cpuFrequencyBoot);
  ctx.show("cpuFrequencySleep", _cpuFrequencySleep);
#endif
#endif // IOTSA_HAS_SLEEP
}

void IotsaRunmodeMod::putHandler(IotsaApiContext& ctx) {
  // As on the web page: the protected fields first, then the open ones
  // (harmless, or confirmed at the device, which is also what BLE allows), and
  // the actions after ctx.failed(). So a refusal comes before anything
  // happened (cwi-dis/iotsa#284, cwi-dis/iotsa#296).
  ctx.argsNeedRight("config");
  bool reboot = false;
  ctx.arg("reboot", reboot, IotsaApiContext::Transient);
  bool wifiDisabled = !iotsaStatus.wifiEnabled;
  bool wifiChanged = ctx.arg("wifiDisabled", wifiDisabled, IotsaApiContext::Transient);
#ifdef IOTSA_WITH_BLE
  bool bleDisabled = !iotsaController.bleRadioWanted();
  bool bleChanged = ctx.arg("bleDisabled", bleDisabled, IotsaApiContext::Transient);
#endif
  ctx.arg("debugLog", iotsaLogDebugEnabled, IotsaApiContext::Saved);
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  int sleepMode = int(_sleepConfig.mode);
  if (ctx.arg("sleepMode", sleepMode, IotsaApiContext::Saved)) {
    _sleepConfig.mode = IotsaSleepMode(sleepMode);
  }
  ctx.arg("sleepDuration", _sleepConfig.sleepDuration, IotsaApiContext::Saved);
  ctx.arg("wakeDuration", _sleepConfig.wakeDuration, IotsaApiContext::Saved);
  ctx.arg("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration, IotsaApiContext::Saved);
  ctx.arg("activityExtraWakeDuration", sp.activityExtraWakeDuration, IotsaApiContext::Saved);
  ctx.arg("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi, IotsaApiContext::Saved);
  ctx.arg("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep, IotsaApiContext::Saved);
  ctx.arg("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower, IotsaApiContext::Saved);
#ifdef ESP32
  ctx.arg("cpuFrequencyBoot", _cpuFrequencyBoot, IotsaApiContext::Saved);
  ctx.arg("cpuFrequencySleep", _cpuFrequencySleep, IotsaApiContext::Saved);
  int cpuFrequency = getCpuFrequencyMhz();
  bool cpuChanged = ctx.arg("cpuFrequency", cpuFrequency, IotsaApiContext::Transient) && cpuFrequency != 0;
#endif
#endif // IOTSA_HAS_SLEEP

  ctx.argsNeedRight(IOTSA_RIGHT_NONE);
  bool identify = false;
  ctx.arg("identify", identify, IotsaApiContext::Transient);
  // -1: not asked. Asking for the same mode again restarts its window.
  int requestedMode = -1;
  bool requestChanged = ctx.arg("requestedMode", requestedMode, IotsaApiContext::Transient);
  if (requestChanged && (requestedMode < IOTSA_MODE_NORMAL || requestedMode > IOTSA_MODE_FACTORY_RESET)) {
    ctx.error(400, "requestedMode: no such mode");
  }
#ifdef IOTSA_HAS_SLEEP
  int postponeSleep = -1;
  bool postponeChanged = ctx.arg("postponeSleep", postponeSleep, IotsaApiContext::Transient);
#endif

  if (ctx.failed()) return;
  if (reboot) iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
  if (wifiChanged) iotsaController.setWifiRadioEnabled(!wifiDisabled);
#ifdef IOTSA_WITH_BLE
  if (bleChanged) iotsaController.setBleRadioEnabled(!bleDisabled);
#endif
#ifdef IOTSA_HAS_SLEEP
#ifdef ESP32
  if (cpuChanged) {
    setCpuFrequencyMhz(cpuFrequency);
    IOTSA_LOG_DEBUG("iotsaRunmode", "CPU frequency %d MHz", cpuFrequency);
  }
#endif
  if (postponeChanged) iotsaController.postponeSleep(postponeSleep);
#endif
  // Acted on in loop(), cwi-dis/iotsa#133.
  if (identify) _pendingIdentify = true;
  if (requestChanged) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106).
    iotsaController.requestMode(iotsa_mode(requestedMode));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      ctx.show("requestedMode", int(iotsaController.requestedMode()));
      ctx.show("requestedModeTimeout", iotsaController.requestedModeRemainingMillis()/1000);
      ctx.show("needsReboot", true);
    }
  }
}

String IotsaRunmodeMod::confirmDescription() {
  String rv;
  if (iotsaController.rcmInteractionDescription()) rv = iotsaController.rcmInteractionDescription();
#ifdef IOTSA_WITH_BLE
  IotsaBLEServerMod *ble = IotsaBLEServerMod::instance();
  if (ble && ble->promoteAllowed()) {
    if (rv.length()) rv += " or ";
    rv += "write promoteMode on the BLE runmode service";
  }
#endif
  return rv;
}

#ifdef IOTSA_WITH_BLE

bool IotsaRunmodeMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == IotsaRunmodeBLE::requestedModeUUID) {
    _pendingBleMode = bleApi.getAsInt(IotsaRunmodeBLE::requestedModeUUID);
    IOTSA_LOG_DEBUG("iotsaRunmode", "BLE: requested mode %d", _pendingBleMode);
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::rebootUUID) {
    if (bleApi.getAsInt(IotsaRunmodeBLE::rebootUUID)) _pendingBleReboot = true;
    IOTSA_LOG_DEBUG("iotsaRunmode", "BLE: reboot requested");
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::promoteModeUUID) {
    if (bleApi.getAsInt(IotsaRunmodeBLE::promoteModeUUID)) {
      IotsaBLEServerMod *ble = IotsaBLEServerMod::instance();
      if (!ble || !ble->promoteAllowed()) {
        IOTSA_LOG_DEBUG("iotsaRunmode", "BLE: promote mode not allowed");
        return false;
      }
      _pendingBlePromoteMode = true;
      IOTSA_LOG_DEBUG("iotsaRunmode", "BLE: promote mode requested");
    }
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::wifiDisabledUUID) {
    _pendingBleWifiDisabled = bleApi.getAsInt(IotsaRunmodeBLE::wifiDisabledUUID) ? 1 : 0;
    IOTSA_LOG_DEBUG("iotsaRunmode", "BLE: wifiDisabled=%d", _pendingBleWifiDisabled);
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::identifyUUID) {
    if (bleApi.getAsInt(IotsaRunmodeBLE::identifyUUID)) _pendingIdentify = true;
    return true;
  }
  return false;
}

bool IotsaRunmodeMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == IotsaRunmodeBLE::currentModeUUID) {
    bleApi.set(IotsaRunmodeBLE::currentModeUUID, (uint8_t)iotsaController.currentMode());
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::requestedModeUUID) {
    bleApi.set(IotsaRunmodeBLE::requestedModeUUID, (uint8_t)iotsaController.requestedMode());
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::wifiDisabledUUID) {
    bleApi.set(IotsaRunmodeBLE::wifiDisabledUUID, (uint8_t)(iotsaStatus.wifiEnabled ? 0 : 1));
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

void IotsaRunmodeMod::_sleepConfigLoad() {
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
    IOTSA_LOG_DEBUG("iotsaRunmode", "CPU frequency %d MHz on boot", _cpuFrequencyBoot);
  }
#endif
  // (millisAtWakeup is left alone -- _sleepTick() seeds it on its first run via
  // noteAwake(). configLoad() runs once at setup() when it's already 0.)
}

void IotsaRunmodeMod::_sleepConfigSave() {
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
  IOTSA_LOG("iotsaRunmode", "saved sleep.cfg");
}

void IotsaRunmodeMod::_notifySleepWakeup(bool sleep) {
  for (IotsaBaseModule* m = app.firstEarlyModule; m != nullptr; m = m->nextModule) m->sleepWakeupNotification(sleep);
  for (IotsaBaseModule* m = app.firstModule; m != nullptr; m = m->nextModule) m->sleepWakeupNotification(sleep);
}

void IotsaRunmodeMod::_sleepTick() {
  IotsaSleepPolicy& sp = iotsaController.sleep();
  if (sp.millisAtWakeup == 0) {
    sp.noteAwake();
#ifdef ESP32
    IOTSA_LOG_DEBUG("iotsaRunmode", "wakeup at %u reason %d", (unsigned)sp.millisAtWakeup, (int)esp_sleep_get_wakeup_cause());
#else
    IOTSA_LOG_DEBUG("iotsaRunmode", "wakeup at %u", (unsigned)sp.millisAtWakeup);
#endif
  }
  if (_sleepConfig.mode == IOTSA_SLEEP_NONE) return;
  if (_pinDisableSleep >= 0 && digitalRead(_pinDisableSleep) == LOW) {
    IOTSA_LOG_DEBUG_SLEEP("iotsaRunmode", "no sleep, pinDisableSleep=%d LOW", _pinDisableSleep);
    return;
  }
  if (!iotsaController.canSleep()) {
    IOTSA_LOG_DEBUG_SLEEP("iotsaRunmode", "no sleep, canSleep() false");
    return;
  }
  IotsaSleepDecision d = sp.decide(_sleepConfig, iotsaStatus.onUsbPower);
  if (d.mode == IOTSA_SLEEP_NONE) return;

  // Committed to sleeping in some form.
  if (_sleepConfig.disableWiFiOnSleep && iotsaStatus.wifiEnabled) {
    static bool haveDisabledWiFi = false;
    if (!haveDisabledWiFi) {
      IOTSA_LOG_DEBUG("iotsaRunmode", "disabling WiFi for sleep");
      haveDisabledWiFi = true;
      iotsaController.setWifiRadioEnabled(false);
      iotsaController.postponeSleep(IotsaSleepPolicy::WIFI_SHUTDOWN_GRACE_MS);
      return;
    }
  }
  IOTSA_LOG_DEBUG("iotsaRunmode", "sleep at %u for %u ms, mode %d", (unsigned)millis(), (unsigned)d.durationMs, (int)d.mode);
  iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_SLEEP, (uint8_t)d.mode);
  // Only now: the WiFi shutdown above returns to loop() first, and used to
  // leave the watchdog paused (cwi-dis/iotsa#244).
  iotsaController.pauseWatchdog();
  _notifySleepWakeup(true);
#ifdef ESP32
  if (_cpuFrequencySleep != 0) {
    static bool haveSetSleepFreq = false;
    if (!haveSetSleepFreq) {
      IOTSA_LOG_DEBUG("iotsaRunmode", "CPU frequency %d MHz for sleep", _cpuFrequencySleep);
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
    iotsaController.resumeWatchdog();
    _notifySleepWakeup(false);
    return;
  }

#ifdef ESP32
  delay(5); // flush serial
  if (d.durationMs) esp_sleep_enable_timer_wakeup(d.durationMs * 1000LL);

  if (d.mode == IOTSA_SLEEP_LIGHT) {
    // Everything stays powered, just runs slowly; we return here after waking.
#ifdef IOTSA_WITH_BLE
    IotsaBLERadioArbiter::pauseAdvertising(IotsaBLERadioArbiter::PAUSE_SLEEP);
#endif
    esp_light_sleep_start();
    sp.noteWokeFromSleep();
    IOTSA_LOG_DEBUG("iotsaRunmode", "light sleep wakeup at %u", (unsigned)sp.millisAtWakeup);
#ifdef IOTSA_WITH_BLE
    // Advertise for the wake window only -- and only if BLE is wanted at all
    // (this used to restart advertising regardless of policy).
    IotsaBLERadioArbiter::resumeAdvertising(IotsaBLERadioArbiter::PAUSE_SLEEP, _sleepConfig.wakeDuration);
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
#ifdef IOTSA_SLEEP_CAN_RTC_PERIPH_POWER_DOMAIN
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
#endif
  }
  esp_deep_sleep_start();
  IOTSA_LOG("iotsaRunmode", "esp_deep_sleep_start() failed");
#else
  // esp8266: only deep sleep, via a full reboot on wake.
  ESP.deepSleep(d.durationMs * 1000LL);
#endif
}
#endif // IOTSA_HAS_SLEEP
