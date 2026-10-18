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

#define SLEEP_DEBUG if(0)
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
// The names are the /api/runmode field names, so the page and REST read alike.
// Auth as in putHandler(): identify and the mode request are open (harmless, or
// confirmed at the device), the rest needs the "config" right (cwi-dis/iotsa#284).

static const char * const requestableModeNames[] = {"Normal", "Configuration", "Over-the-air update"};
#ifdef IOTSA_HAS_SLEEP
static const char * const sleepModeNames[] = {"None", "Delay", "Light sleep", "Deep sleep", "Hibernate"};
#endif

void IotsaRunmodeMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.needsRight(IOTSA_RIGHT_NONE);
  if (ctx.action("identify")) {
    _pendingIdentify = true;
    ctx.feedback("Identifying.");
  }
  // Factory reset is not one of the choices: it has its own confirmed button.
  int requested = int(iotsaController.requestedMode());
  if (ctx.choice("requestedMode", requested, _otaAvailable() ? 3 : 2, IotsaWebContext::Live)) {
    iotsaController.requestMode(iotsa_mode(requested));
  }
  if (ctx.confirmedAction("factoryReset")) {
    iotsaController.requestMode(IOTSA_MODE_FACTORY_RESET);
  }

  ctx.needsRight("config");
  if (ctx.action("reboot")) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    ctx.feedback("Rebooting in 2 seconds.");
  }
  bool wifiDisabled = !iotsaStatus.wifiEnabled;
  if (ctx.arg("wifiDisabled", wifiDisabled, IotsaWebContext::Live)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled = !iotsaController.bleRadioWanted();
  if (ctx.arg("bleDisabled", bleDisabled, IotsaWebContext::Live)) {
    iotsaController.setBleRadioEnabled(!bleDisabled);
  }
#endif
  ctx.checkbox("debugLog", iotsaLogDebugEnabled);
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  int sleepMode = int(_sleepConfig.mode);
  if (ctx.choice("sleepMode", sleepMode, _IOTSA_SLEEP_MAX)) {
    _sleepConfig.mode = IotsaSleepMode(sleepMode);
  }
  ctx.arg("sleepDuration", _sleepConfig.sleepDuration);
  ctx.arg("wakeDuration", _sleepConfig.wakeDuration);
  ctx.arg("activityExtraWakeDuration", sp.activityExtraWakeDuration);
  ctx.arg("bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration);
  ctx.checkbox("disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi);
  ctx.checkbox("disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep);
  ctx.checkbox("disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower);
#ifdef ESP32
  ctx.arg("cpuFrequencyBoot", _cpuFrequencyBoot);
  ctx.arg("cpuFrequencySleep", _cpuFrequencySleep);
  int cpuFrequency = getCpuFrequencyMhz();
  if (ctx.arg("cpuFrequency", cpuFrequency, IotsaWebContext::Live) && cpuFrequency != 0) {
    setCpuFrequencyMhz(cpuFrequency);
  }
#endif
#endif // IOTSA_HAS_SLEEP
}

void IotsaRunmodeMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section == IotsaWebContext::Everyday) {
    String current = iotsaController.modeName(iotsaController.currentMode());
    if (iotsaController.currentMode()) {
      current += ", times out in " + String((iotsaController.currentModeEndTime() - millis())/1000) + " seconds";
    }
    ctx.status("Current mode", "currentMode", current);
    if (iotsaController.requestedMode()) {
      String requested = iotsaController.modeName(iotsaController.requestedMode());
      requested += ", enable within " + String((iotsaController.requestedModeEndTime() - millis())/1000) + " seconds by power cycling";
      if (iotsaController.rcmInteractionDescription()) {
        requested += " or " + String(iotsaController.rcmInteractionDescription());
      }
      ctx.status("Requested mode", "requestedMode", requested);
    }
    ctx.status("Configuration mode timeout", "", String(iotsaController.modeTimeout()) + " seconds");
    ctx.status("WiFi radio", "wifiDisabled", iotsaStatus.wifiEnabled ? "enabled" : "disabled");
#ifdef IOTSA_WITH_BLE
    ctx.status("BLE radio", "bleDisabled", iotsaController.bleRadioWanted() ? "enabled" : "disabled");
#endif

    ctx.button("identify", "Identify");
    ctx.button("reboot", "Reboot now");
    ctx.button("wifiDisabled", iotsaStatus.wifiEnabled ? "Disable WiFi radio" : "Enable WiFi radio", iotsaStatus.wifiEnabled ? "1" : "0");
#ifdef IOTSA_WITH_BLE
    ctx.button("bleDisabled", iotsaController.bleRadioWanted() ? "Disable BLE radio" : "Enable BLE radio", iotsaController.bleRadioWanted() ? "1" : "0");
#endif
    {
      IotsaWebContext::Form form(ctx, "Request", "Mode after the next reboot");
      int requested = int(iotsaController.requestedMode());
      ctx.choice("Mode", "requestedMode", requested, requestableModeNames, _otaAvailable() ? 3 : 2, IotsaWebContext::Live);
    }
    ctx.confirmedButton("factoryReset", "Factory reset and clear all files");
  } else {
    {
      IotsaWebContext::Form form(ctx, "Save", "Debug logging");
      ctx.checkbox("Debug logging", "debugLog", iotsaLogDebugEnabled);
    }
#ifdef IOTSA_HAS_SLEEP
    IotsaSleepPolicy& sp = iotsaController.sleep();
    IotsaWebContext::Form form(ctx, "Save", "Sleep / wake");
    ctx.choice("Sleep mode", "sleepMode", int(_sleepConfig.mode), sleepModeNames, _IOTSA_SLEEP_MAX);
    ctx.field("Sleep duration", "sleepDuration", _sleepConfig.sleepDuration, IotsaWebContext::Config, "ms");
    ctx.field("Wake duration", "wakeDuration", _sleepConfig.wakeDuration, IotsaWebContext::Config, "ms");
    ctx.field("Extra wake after activity", "activityExtraWakeDuration", sp.activityExtraWakeDuration, IotsaWebContext::Config, "ms");
    ctx.field("Extra wake after power-on or reset", "bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration, IotsaWebContext::Config, "ms");
    ctx.checkbox("Don't sleep while WiFi is active", "disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi);
    ctx.checkbox("Disable WiFi before sleeping", "disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep);
    ctx.checkbox("Don't sleep on USB power", "disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower);
#ifdef ESP32
    ctx.field("CPU frequency on boot", "cpuFrequencyBoot", _cpuFrequencyBoot, IotsaWebContext::Config, "MHz, 0: unchanged");
    ctx.field("CPU frequency on first sleep", "cpuFrequencySleep", _cpuFrequencySleep, IotsaWebContext::Config, "MHz, 0: unchanged");
    ctx.field("Current CPU frequency", "cpuFrequency", getCpuFrequencyMhz(), IotsaWebContext::Live, "MHz");
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
    message += String((iotsaController.requestedModeEndTime() - millis())/1000);
    message += " seconds by power cycling";
    if (iotsaController.rcmInteractionDescription()) {
      message += " or ";
      message += iotsaController.rcmInteractionDescription();
    }
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

// Which requests need the application's auth provider (cwi-dis/iotsa#284). Reads
// are open (status and mode information, nothing secret). So are the writes that
// are harmless or confirmed at the device itself, which is also what BLE allows:
// identify, postponeSleep, and requestedMode (a requested mode only takes effect
// after a reset at the device). reboot, wifiDisabled, bleDisabled, the CPU clock
// and the saved sleep settings need the "config" right. So the REST transport's
// per-endpoint check is switched off here, and putHandler() checks the request
// itself, before executing anything. The web page applies the same rules.
bool IotsaRunmodeMod::needsAuthentication(const char *obj, IotsaApiOperation verb) {
  return false;
}

static bool isOpenField(const char *key) {
  return strcmp(key, "identify") == 0 || strcmp(key, "requestedMode") == 0 || strcmp(key, "postponeSleep") == 0;
}

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

bool IotsaRunmodeMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/status") == 0) {
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
    return true;
  }
  reply["currentMode"] = int(iotsaController.currentMode());
  if (iotsaController.currentMode()) {
    reply["currentModeTimeout"] = (iotsaController.currentModeEndTime() - millis())/1000;
  }
  reply["requestedMode"] = int(iotsaController.requestedMode());
  if (iotsaController.requestedMode()) {
    reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
  }
  reply["wifiDisabled"] = !iotsaStatus.wifiEnabled;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabled"] = !iotsaController.bleRadioWanted();
#endif
  reply["identifyAvailable"] = !_identifyCallbacks.empty();   // cwi-dis/iotsa#133
  reply["debugLog"] = iotsaLogDebugEnabled;   // cwi-dis/iotsa#182
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

bool IotsaRunmodeMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  JsonObject reqObj = request.as<JsonObject>();
  // Anything beyond the open fields needs the config right, and then the whole
  // request is refused before anything is executed (cwi-dis/iotsa#284).
  for (JsonPair kv : reqObj) {
    if (!isOpenField(kv.key().c_str())) {
      if (needsAuthentication("config")) return apiError(401, "needs the config right");
      break;
    }
  }

  bool wifiDisabled;
  if (getFromRequest<int>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<int>(reqObj, "bleDisabled", bleDisabled)) {
    iotsaController.setBleRadioEnabled(!bleDisabled);
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    if (reqModeInt < IOTSA_MODE_NORMAL || reqModeInt > IOTSA_MODE_FACTORY_RESET) return apiError(400, "requestedMode: no such mode");
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106).
    iotsaController.requestMode(iotsa_mode(reqModeInt));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      reply["requestedMode"] = int(iotsaController.requestedMode());
      reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  bool flag;
  if (getFromRequest<bool>(reqObj, "reboot", flag) && flag) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
  }
  if (getFromRequest<bool>(reqObj, "identify", flag) && flag) {
    _pendingIdentify = true;   // cwi-dis/iotsa#133; acted on in loop(), no auth
  }
  if (getFromRequest<bool>(reqObj, "debugLog", iotsaLogDebugEnabled)) {
    configSave();
    if (changed) *changed = true;
  }
#ifdef IOTSA_HAS_SLEEP
  IotsaSleepPolicy& sp = iotsaController.sleep();
  bool sleepChanged = false;
  int intValue;
  if (getFromRequest<int>(reqObj, "postponeSleep", intValue)) {
    iotsaController.postponeSleep(intValue);
  }
  if (getFromRequest<int>(reqObj, "sleepMode", intValue))              { _sleepConfig.mode = (IotsaSleepMode)intValue; sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "sleepDuration", _sleepConfig.sleepDuration))  { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "wakeDuration", _sleepConfig.wakeDuration))    { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "bootExtraWakeDuration", _sleepConfig.bootExtraWakeDuration)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "activityExtraWakeDuration", sp.activityExtraWakeDuration)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "disableSleepOnWiFi", _sleepConfig.disableSleepOnWiFi)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "disableWiFiOnSleep", _sleepConfig.disableWiFiOnSleep)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "disableSleepOnUSBPower", _sleepConfig.disableSleepOnUSBPower)) { sleepChanged = true; }
#ifdef ESP32
  if (getFromRequest<int>(reqObj, "cpuFrequencyBoot", _cpuFrequencyBoot)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "cpuFrequencySleep", _cpuFrequencySleep)) { sleepChanged = true; }
  if (getFromRequest<int>(reqObj, "cpuFrequency", intValue)) {
    setCpuFrequencyMhz(intValue);
    IFDEBUG IotsaSerial.printf("Set CPU frequency to %d MHz\n", intValue);
  }
#endif
  if (sleepChanged) {
    configSave();
    if (changed) *changed = true;
  }
#endif // IOTSA_HAS_SLEEP
  checkUnhandled(reqObj);
  return true;
}

#ifdef IOTSA_WITH_BLE
void IotsaRunmodeMod::allowBLEModeSwitch() {
  _bleAllowModeSwitch = true;
  iotsaController.allowRCMDescription("write promoteMode on the BLE runmode service");
}

bool IotsaRunmodeMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == IotsaRunmodeBLE::requestedModeUUID) {
    _pendingBleMode = bleApi.getAsInt(IotsaRunmodeBLE::requestedModeUUID);
    IFDEBUG IotsaSerial.printf("runmode: BLE requested mode %d\n", _pendingBleMode);
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::rebootUUID) {
    if (bleApi.getAsInt(IotsaRunmodeBLE::rebootUUID)) _pendingBleReboot = true;
    IFDEBUG IotsaSerial.println("runmode: BLE reboot requested");
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::promoteModeUUID) {
    if (bleApi.getAsInt(IotsaRunmodeBLE::promoteModeUUID)) {
      if (_bleAllowModeSwitch) {
        _pendingBlePromoteMode = true;
        IFDEBUG IotsaSerial.println("runmode: BLE promote-mode requested");
      } else {
        IFDEBUG IotsaSerial.println("runmode: BLE promote-mode requested but not allowed");
      }
    }
    return true;
  }
  if (charUUID == IotsaRunmodeBLE::wifiDisabledUUID) {
    _pendingBleWifiDisabled = bleApi.getAsInt(IotsaRunmodeBLE::wifiDisabledUUID) ? 1 : 0;
    IFDEBUG IotsaSerial.printf("runmode: BLE wifiDisabled=%d\n", _pendingBleWifiDisabled);
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
    IFDEBUG IotsaSerial.printf("Set CPU frequency to %d MHz on boot\n", _cpuFrequencyBoot);
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
  iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_SLEEP, (uint8_t)d.mode);
  // Only now: the WiFi shutdown above returns to loop() first, and used to
  // leave the watchdog paused (cwi-dis/iotsa#244).
  iotsaController.pauseWatchdog();
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
    iotsaController.resumeWatchdog();
    _notifySleepWakeup(false);
    return;
  }

#ifdef ESP32
  IFDEBUG delay(5); // flush serial
  if (d.durationMs) esp_sleep_enable_timer_wakeup(d.durationMs * 1000LL);

  if (d.mode == IOTSA_SLEEP_LIGHT) {
    // Everything stays powered, just runs slowly; we return here after waking.
#ifdef IOTSA_WITH_BLE
    IotsaBLERadioArbiter::pauseAdvertising(IotsaBLERadioArbiter::PAUSE_SLEEP);
#endif
    esp_light_sleep_start();
    sp.noteWokeFromSleep();
    IFDEBUG IotsaSerial.printf("light sleep wakeup at %u\n", (unsigned)sp.millisAtWakeup);
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
  IotsaSerial.println("esp_deep_sleep_start() failed?");
#else
  // esp8266: only deep sleep, via a full reboot on wake.
  ESP.deepSleep(d.durationMs * 1000LL);
#endif
}
#endif // IOTSA_HAS_SLEEP
