#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"
#ifdef ESP32
#include <esp_wifi.h>
#include <esp_bt.h>
#if ESP_ARDUINO_VERSION_MAJOR > 2
#include "esp_system.h"
#include "rom/ets_sys.h"
#endif
#endif

#define SLEEP_DEBUG if(0)
#ifdef ESP32
// the watchdog timer, for rebooting on hangs
hw_timer_t *watchdogTimer = NULL;

void IRAM_ATTR watchdogTimerTriggered() {
  ets_printf("iotsa watchdog reboot");
  esp_restart();
}
#endif

#ifdef IOTSA_WITH_WEB
void
IotsaBatteryMod::handler() {
  bool anyChanged = false;
  if( server->hasArg("sleepDuration")) {
    if (needsAuthentication()) return;
    sleepDuration = server->arg("sleepDuration").toInt();
    anyChanged = true;
  }
  if( server->hasArg("sleepMode")) {
    if (needsAuthentication()) return;
    sleepMode = (IotsaSleepMode)server->arg("sleepMode").toInt();
    anyChanged = true;
  }
  if( server->hasArg("wakeDuration")) {
    if (needsAuthentication()) return;
    wakeDuration = server->arg("wakeDuration").toInt();
    anyChanged = true;
  }
  
#ifdef ESP32
  if( server->hasArg("watchdogDuration")) {
    if (needsAuthentication()) return;
    watchdogDuration = server->arg("watchdogDuration").toInt();
    anyChanged = true;
  }
#endif
  if( server->hasArg("bootExtraWakeDuration")) {
    if (needsAuthentication()) return;
    bootExtraWakeDuration = server->arg("bootExtraWakeDuration").toInt();
    anyChanged = true;
  }
  if( server->hasArg("activityExtraWakeDuration")) {
    if (needsAuthentication()) return;
    iotsaConfig.activityExtraWakeDuration = server->arg("activityExtraWakeDuration").toInt();
    anyChanged = true;
  }
  if( server->hasArg("disableSleepOnUSBPower")) {
    if (needsAuthentication()) return;
    disableSleepOnUSBPower = server->arg("disableSleepOnUSBPower").toInt();
    anyChanged = true;
  }
  if( server->hasArg("disableSleepOnWiFi")) {
    if (needsAuthentication()) return;
    disableSleepOnWiFi = server->arg("disableSleepOnWiFi").toInt();
    anyChanged = true;
  }
  if( server->hasArg("correctionVBat")) {
    if (needsAuthentication()) return;
    correctionVBat = server->arg("correctionVBat").toFloat();
    anyChanged = true;
  }
  if (anyChanged) {
    iotsaConfig.extendCurrentMode();
    configSave();
  }

  String message = "<html><head><title>Battery power saving module</title></head><body><h1>Battery power saving module</h1>";
  _readVoltages();
  message += "<p>Wakeup time: " + String(millisAtWakeup) + "ms<br>";
#ifdef ESP32
  message += "Wakeup reason: " + String(esp_sleep_get_wakeup_cause()) + "<br>";
#endif
  message += "Awake for: " + String((millis() - millisAtWakeup)/1000.0) + "s<br>";
  if (sleepMode && wakeDuration) {
    uint32_t nextSleepTime = millisAtWakeup + wakeDuration;
    if (!didWakeFromSleep) nextSleepTime += bootExtraWakeDuration;
    long remainAwakeMillis =  nextSleepTime - millis();
    message += "Remaining awake for: " + String(remainAwakeMillis/1000.0) + "s<br>";
  }
  if (pinVBat >= 0) {
    message += "Battery level: " + String(levelVBat) + "%<br>";
  }
  if (pinVUSB >= 0) {
    message += "USB voltage level: " + String(levelVUSB) + "%<br>";
  }
  message += "</p>";
  message += "<form method='post'>";
  message += "Sleep mode: <select name='sleepMode' value='" + String(sleepMode) + 
    "'><option value='0'" + String(sleepMode==IOTSA_SLEEP_NONE?" selected":"") + 
    ">None</option><option value='1'" + String(sleepMode==IOTSA_SLEEP_DELAY?" selected":"") + 
    ">Delay</option><option value='2'" + String(sleepMode==IOTSA_SLEEP_LIGHT?" selected":"") + 
    ">Light sleep</option><option value='3'" + String(sleepMode==IOTSA_SLEEP_DEEP?" selected":"") + 
    ">Deep sleep</option><option value='4'" + String(sleepMode==IOTSA_SLEEP_HIBERNATE?" selected":"") + 
    ">Hibernate</option></select><br>";
  message += "Sleep duration (ms): <input name='sleepDuration' value='" + String(sleepDuration) + "'><br>";
  message += "Wake duration (ms): <input name='wakeDuration' value='" + String(wakeDuration) + "'><br>";
  message += "Extra wake duration after activity (ms): <input name='activityExtraWakeDuration' value='" + String(iotsaConfig.activityExtraWakeDuration) + "'><br>";
  message += "Extra wake duration after poweron/reset (ms): <input name='bootExtraWakeDuration' value='" + String(bootExtraWakeDuration) + "'><br>";
  if (pinVUSB >= 0) {
    message += "<input type='radio' name='disableSleepOnUSBPower' value='0'" + String(disableSleepOnUSBPower?"":" checked") + ">Sleep on both USB or battery power<br>";
    message += "<input type='radio' name='disableSleepOnUSBPower' value='1'" + String(disableSleepOnUSBPower?" checked":"") + ">Disable sleep on USB power<br>";
  }
  message += "<input type='radio' name='disableSleepOnWiFi' value='0'" + String(disableSleepOnWiFi?"":" checked") + ">Sleep independent of WiFi<br>";
  message += "<input type='radio' name='disableSleepOnWiFi' value='1'" + String(disableSleepOnWiFi?" checked":"") + ">Disable sleep if WiFi is active<br>";
  if (pinVBat >= 0) {
    message += "Battery voltage correction factor: <input name='correctionVBat' value='" + String(correctionVBat) + "'><br>";
  }
#ifdef ESP32
  message += "Watchdog timer duration (ms): <input name='watchdogDuration' value='" + String(watchdogDuration) + "'><br>";
#endif
  message += "<input type='submit'></form>";
  server->send(200, "text/html", message);
}

String IotsaBatteryMod::info() {
  String message = "<p>Built with battery module. See <a href=\"/battery\">/battery</a> to change the battery power saving options.";
#ifdef IOTSA_WITH_API
  message += " Or access the REST interface at <a href='/api/battery'>/api/battery</a>.";
#endif
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaBatteryMod::setup() {
  configLoad();
#ifdef ESP32
  didWakeFromSleep = (esp_sleep_get_wakeup_cause() != 0);
  
  if (watchdogDuration) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
    watchdogTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(watchdogTimer, &watchdogTimerTriggered, true);
    timerAlarmWrite(watchdogTimer, watchdogDuration*1000, false);
    timerAlarmEnable(watchdogTimer);
#else
    watchdogTimer = timerBegin(1000000);
    timerAttachInterrupt(watchdogTimer, &watchdogTimerTriggered);
    timerAlarm(watchdogTimer, watchdogDuration*1000, true, 0);
#endif
    IFDEBUG IotsaSerial.printf("Watchdog: %d ms\n", watchdogDuration);
  }
#endif
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  bleApi.addCharacteristic(levelVBatUUID, BLE_READ, BLE2904::FORMAT_UINT8, 0x27AD, "Battery Level");
  bleApi.addCharacteristic(levelVUSBUUID, BLE_READ, BLE2904::FORMAT_UINT8, 0x27AD, "USB Voltage Level");
  bleApi.addCharacteristic(doSoftRebootUUID, BLE_WRITE, BLE2904::FORMAT_UINT8, 0x2700, "Reboot with WiFi");
#endif
  iotsaConfig.setExtensionCallback(std::bind(&IotsaBatteryMod::extendCurrentMode, this));
}

void IotsaBatteryMod::allowBLEConfigModeSwitch() {
  bleConfigModeSwitchAllowed = true;
  iotsaConfig.allowRCMDescription("use BLE to set 'reboot with WiFi' to 2");
}

#ifdef IOTSA_WITH_API
bool IotsaBatteryMod::getHandler(const char *path, JsonObject& reply) {
  reply["sleepMode"] = (int)sleepMode;
  reply["sleepDuration"] = sleepDuration;
  reply["wakeDuration"] = wakeDuration;
  reply["bootExtraWakeDuration"] = bootExtraWakeDuration;
  reply["activityExtraWakeDuration"] = iotsaConfig.activityExtraWakeDuration;
  reply["postponeSleep"] = iotsaConfig.postponeSleep(0);
#ifdef ESP32
  reply["watchdogDuration"] = watchdogDuration;
#endif
  _readVoltages();
  if (pinVBat >= 0) {
    reply["levelVBat"] = levelVBat;
    reply["correctionVBat"] = correctionVBat;
  }
  if (pinVUSB >= 0) {
    reply["levelVUSB"] = levelVUSB;
    reply["disableSleepOnUSBPower"] = disableSleepOnUSBPower;
  }
  reply["disableSleepOnWiFi"] = disableSleepOnWiFi;
  return true;
}

bool IotsaBatteryMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj["postponeSleep"].is<int>()) {
    iotsaConfig.postponeSleep(reqObj["postponeSleep"].as<int>());
  }
  if (reqObj["sleepMode"].is<int>()) {
    sleepMode = (IotsaSleepMode)reqObj["sleepMode"].as<int>();
    anyChanged = true;
  }
  if (reqObj["sleepDuration"].is<int>()) {
    sleepDuration = reqObj["sleepDuration"];
    anyChanged = true;
  }
  if (reqObj["wakeDuration"].is<int>()) {
    wakeDuration = reqObj["wakeDuration"];
    anyChanged = true;
  }
  if (reqObj["bootExtraWakeDuration"].is<int>()) {
    bootExtraWakeDuration = reqObj["bootExtraWakeDuration"];
    anyChanged = true;
  }
  if (reqObj["activityExtraWakeDuration"].is<int>()) {
    iotsaConfig.activityExtraWakeDuration = reqObj["activityExtraWakeDuration"];
    anyChanged = true;
  }
#ifdef ESP32
  if (reqObj["watchdogDuration"].is<int>()) {
    watchdogDuration = reqObj["watchdogDuration"];
    anyChanged = true;
  }
#endif
  if (pinVBat >= 0 && reqObj["correctionVBat"].is<float>()) {
    correctionVBat = reqObj["correctionVBat"];
    anyChanged = true;
  }
  if (pinVUSB >= 0 && reqObj["disableSleepOnUSBPower"].is<bool>()) {
    disableSleepOnUSBPower = reqObj["disableSleepOnUSBPower"];
    anyChanged = true;
  }
  if (reqObj["disableSleepOnWiFi"].is<bool>()) {
    disableSleepOnWiFi = reqObj["disableSleepOnWiFi"];
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // IOTSA_WITH_API

#ifdef IOTSA_WITH_BLE
bool IotsaBatteryMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == doSoftRebootUUID) {
      doSoftReboot = bleApi.getAsInt(doSoftRebootUUID);
      IFDEBUG IotsaSerial.printf("request reboot mode %d\n", doSoftReboot);
      IFDEBUG IotsaSerial.println(doSoftReboot);
      return true;
  }
  return false;
}

bool IotsaBatteryMod::bleGetHandler(UUIDstring charUUID) {
  _readVoltages();
  if (charUUID == levelVBatUUID) {
      bleApi.set(levelVBatUUID, levelVBat);
      return true;
  }
  if (charUUID == levelVUSBUUID) {
      bleApi.set(levelVUSBUUID, levelVUSB);
      return true;
  }
  return false;
}
#endif // IOTSA_WITH_BLE

void IotsaBatteryMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/battery", std::bind(&IotsaBatteryMod::handler, this));
  server->on("/battery", HTTP_POST, std::bind(&IotsaBatteryMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/battery", true, true);
  name = "battery";
#endif
}

void IotsaBatteryMod::configLoad() {
  IotsaConfigFileLoad cf("/config/battery.cfg");
  int value;
  cf.get("sleepMode", value, 0);
  if (value > _IOTSA_SLEEP_MAX) value = IOTSA_SLEEP_NONE;
  sleepMode = (IotsaSleepMode)value;
  cf.get("wakeDuration", wakeDuration, 0);
  cf.get("bootExtraWakeDuration", bootExtraWakeDuration, 0);
  cf.get("activityExtraWakeDuration", iotsaConfig.activityExtraWakeDuration, 0);
  cf.get("sleepDuration", sleepDuration, 0);
#ifdef ESP32
  cf.get("watchdogDuration", watchdogDuration, 0);
#endif
  cf.get("correctionVBat", correctionVBat, 1.0);
  cf.get("disableSleepOnUSBPower", disableSleepOnUSBPower, 0);
  cf.get("disableSleepOnWiFi", disableSleepOnWiFi, 0);
  millisAtWakeup = 0;
}

void IotsaBatteryMod::configSave() {
  IotsaConfigFileSave cf("/config/battery.cfg");
  cf.put("sleepMode", sleepMode);
  cf.put("wakeDuration", wakeDuration);
  cf.put("bootExtraWakeDuration", bootExtraWakeDuration);
  cf.put("activityExtraWakeDuration", iotsaConfig.activityExtraWakeDuration);
  cf.put("sleepDuration", sleepDuration);
#ifdef ESP32
  cf.put("watchdogDuration", watchdogDuration);
#endif
  if (pinVBat >= 0) {
    cf.put("correctionVBat", correctionVBat);
  }
  if (pinVUSB >= 0) {
    cf.put("disableSleepOnUSBPower", disableSleepOnUSBPower);
  }
  cf.put("disableSleepOnWiFi", disableSleepOnWiFi);
  millisAtWakeup = 0;
}

void IotsaBatteryMod::extendCurrentMode() {
#ifdef ESP32
  if (watchdogTimer) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
    timerWrite(watchdogTimer, 0);
#else
    timerAlarm(watchdogTimer, watchdogDuration*1000, false, 0);
#endif
  }
#endif
  millisAtWakeup = millis();
  SLEEP_DEBUG IotsaSerial.println("Battery: extend mode");
}

void IotsaBatteryMod::_notifySleepWakeup(bool sleep) {
  for(IotsaBaseMod* m=app.firstEarlyModule; m != nullptr; m=m->nextModule) {
    m->sleepWakeupNotification(sleep);
  }
  for(IotsaBaseMod* m=app.firstModule; m != nullptr; m=m->nextModule) {
    m->sleepWakeupNotification(sleep);
  }

}

void IotsaBatteryMod::loop() {
#ifdef ESP32
  if (watchdogTimer) {
    timerWrite(watchdogTimer, 0);
  }
#endif
  if (millisAtWakeup == 0) {
    millisAtWakeup = millis();
    IFDEBUG IotsaSerial.print("wakeup at ");
    IFDEBUG IotsaSerial.print(millisAtWakeup);
#ifdef ESP32
    IFDEBUG IotsaSerial.print(" reason ");
    IFDEBUG IotsaSerial.println(esp_sleep_get_wakeup_cause());
#endif
    _readVoltages();
  }
  // If a reboot or configuration mode change has been requested (probably over BLE) we do so now.
  if (doSoftReboot) {
    if (doSoftReboot == 2) {
      if (bleConfigModeSwitchAllowed) {
        IFDEBUG IotsaSerial.println("Allow configmode change from BLE");
        iotsaConfig.allowRequestedConfigurationMode();
      } else {
        IFDEBUG IotsaSerial.println("Configmode change from BLE requested but not allowed");
      }
    } else if (doSoftReboot == 3) {
      // Enable WiFi
      iotsa_wifi_mode newMode = iotsa_wifi_mode::IOTSA_WIFI_NORMAL;
      iotsaConfig.wifiMode = newMode;
      iotsaConfig.wantWifiModeSwitchAtMillis = millis()+1000;
      IFDEBUG IotsaSerial.println("Enable WiFi from BLE");
   
    } else {
      // doSoftReboot is probably 1. Reboot.
      IFDEBUG IotsaSerial.println("Reboot from BLE");
      ESP.restart();
    }
    doSoftReboot = 0;
  }
  // Return quickly if no sleep or wifi sleep is required.
  if (sleepMode == IOTSA_SLEEP_NONE) return;
  // Check whether we should disable Wifi or sleep
  int curWakeDuration = wakeDuration;
  if (!didWakeFromSleep) curWakeDuration += bootExtraWakeDuration;
  bool shouldSleep = sleepMode && curWakeDuration > 0 && millis() > millisAtWakeup + curWakeDuration;
  // Again, return quickly if no sleep or wifi sleep is required.
  if (!shouldSleep) return;
  // Now check for other reasons NOT to go to sleep or disable wifi
  if (disableSleepOnWiFi && iotsaConfig.wifiMode != iotsa_wifi_mode::IOTSA_WIFI_DISABLED) {
    return;
  }
  if (pinDisableSleep >= 0 && digitalRead(pinDisableSleep) == LOW) {
    shouldSleep = false;
    SLEEP_DEBUG IotsaSerial.printf("iotsaBattery: no sleep, pinDisableSleep=%d level LOW\n", pinDisableSleep);
  }
  // Another reason is if we're running on USB power and we only sleep on battery power
  if (disableSleepOnUSBPower && pinVUSB >= 0) {
    if (levelVUSB > 80) {
      shouldSleep = false;
      SLEEP_DEBUG IotsaSerial.printf("iotsaBattery: no sleep, USB power\n");
    }
  }
  // Another reason is that we are in configuration mode
  if (iotsaConfig.inConfigurationMode()) {
      SLEEP_DEBUG IotsaSerial.printf("iotsaBattery: no sleep, in configuration mode\n");
      shouldSleep = false;
  }
  // A final reason is if some other module is asking for an extension of the waking period.
  // This does not extend wifi duration, though.
  if (!iotsaConfig.canSleep()) {
      shouldSleep = false;
      SLEEP_DEBUG IotsaSerial.printf("iotsaBattery: no sleep, canSleep() return false\n");
  }
  
  if (!shouldSleep) return;
#ifdef ESP32
  if (watchdogTimer) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
    timerAlarmDisable(watchdogTimer);
#else
    timerDetachInterrupt(watchdogTimer);
#endif
  }
#endif
  // We go to sleep, in some form.
  IFDEBUG IotsaSerial.print("Going to sleep at ");
  IFDEBUG IotsaSerial.print(millis());
  IFDEBUG IotsaSerial.print(" for ");
  IFDEBUG IotsaSerial.print(sleepDuration);
  IFDEBUG IotsaSerial.print(" mode ");
  IFDEBUG IotsaSerial.println(sleepMode);
  _notifySleepWakeup(true);
  if(sleepMode == IOTSA_SLEEP_DELAY) {
    // This isn't really sleeping, it's just a delay. Not sure it is actually useful.
    delay(sleepDuration);
    millisAtWakeup = 0;
    didWakeFromSleep = true;
#ifdef ESP32
    if (watchdogTimer) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
      timerWrite(watchdogTimer, 0);
      timerAlarmEnable(watchdogTimer);
#else
      timerAlarm(watchdogTimer, watchdogDuration*1000, true, 0);
#endif
    }
#endif
    _notifySleepWakeup(false);
    return;
  }
#ifdef ESP32
  // We are going to sleep. First set the reasons for wakeup, such as a timer.
  IFDEBUG delay(5); // Flush serial buffer
  if (sleepDuration) {
    esp_sleep_enable_timer_wakeup(sleepDuration*1000LL);
  } else {
    // xxxjack configure other wakeup sources...
  }
  if (sleepMode == IOTSA_SLEEP_LIGHT) {
    // Light sleep is easiest: everything remains powered just running slowly.
    // We return here after the sleep.
#ifdef IOTSA_WITH_BLE
    bool btActive = IotsaBLEServerMod::pauseServer();
#endif
    esp_light_sleep_start();
    IFDEBUG IotsaSerial.print("light sleep wakup at ");
    millisAtWakeup = millis();
    didWakeFromSleep = true;
    IFDEBUG IotsaSerial.println(millisAtWakeup);
#ifdef IOTSA_WITH_BLE
    if (btActive) {
      IFDEBUG IotsaSerial.println("Re-activate ble");
      IotsaBLEServerMod::resumeServer();
    }
#endif
    if (watchdogTimer) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
      timerWrite(watchdogTimer, 0);
      timerAlarmEnable(watchdogTimer);
#else
      timerWrite(watchdogTimer, 0);
      timerAlarm(watchdogTimer, watchdogDuration*1000, false, 0);
#endif
    }
    _notifySleepWakeup(false);
    return;
  }
  // Before sleeping we turn off the radios.
  if (iotsaConfig.wifiEnabled) esp_wifi_stop();
  esp_bt_controller_disable();
  // For hibernation we also turn off various peripherals.
  if (sleepMode == IOTSA_SLEEP_HIBERNATE) {
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  }
  // Time to go to sleep.
  esp_deep_sleep_start();
  // We should not return here, but get a reboot later.
  IotsaSerial.println("esp_deep_sleep_start() failed?");
#else
  // For esp8266 only deep-sleep is implemented.
  ESP.deepSleep(sleepDuration*1000LL);
#endif
}

void IotsaBatteryMod::_readVoltages() {
  if (pinVBat >= 0) {
    int level = analogRead(pinVBat);
    // 3.9v input would give a reading of 4095 (at the default attenuation of 11dB). We scale, so a voltage of rangeVBat gives 100%
    // See https://esphome.io/components/sensor/adc.html#adc-esp32-attenuation for a description of the attenuation
    float lvbFloat = (level * correctionVBat * 3.9)/ 4096.0;
    float charge = (lvbFloat - rangeVBatMin) / (rangeVBat - rangeVBatMin);
    levelVBat = charge <= 0 ? 0 : int(100*charge);
    //IFDEBUG IotsaSerial.printf("analog level=%d float level=%f range=%f..%f charge=%f levelVBat=%d\n", level, lvbFloat, rangeVBatMin, rangeVBat, charge, levelVBat);
    IFDEBUG IotsaSerial.print("VBat=");
    IFDEBUG IotsaSerial.println(levelVBat);
  }
  if (pinVUSB >= 0) {
    int level = analogRead(pinVUSB);
    levelVUSB = int(100*3.9*level/(rangeVUSB*4096));
//    IFDEBUG IotsaSerial.printf("analog level=%d levelVUSB=%d\n", level, levelVUSB);
  }
}