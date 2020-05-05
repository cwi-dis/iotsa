#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"
#ifdef ESP32
#include <esp_wifi.h>
#include <esp_bt.h>
#endif

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
  if( server->hasArg("wifiActiveDuration")) {
    if (needsAuthentication()) return;
    wifiActiveDuration = server->arg("wifiActiveDuration").toInt();
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
  if( server->hasArg("disableSleepOnUSBPower")) {
    if (needsAuthentication()) return;
    disableSleepOnUSBPower = server->arg("disableSleepOnUSBPower").toInt();
    anyChanged = true;
  }
  if (anyChanged) configSave();

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
    message += "Remaining awake for: " + String((nextSleepTime - millis())/1000.0) + "s<br>";
  }
  if (wifiActiveDuration) {
    message += "Wifi will be disabled in: " + String((millisAtWifiWakeup + wifiActiveDuration - millis())/1000.0) + "s<br>";
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
  message += "Extra wake duration after poweron/reset (ms): <input name='bootExtraWakeDuration' value='" + String(bootExtraWakeDuration) + "'><br>";
  message += "WiFi duration after poweron/reset/deepsleep (ms): <input name='wifiActiveDuration' value='" + String(wifiActiveDuration) + "'><br>";
  if (pinVUSB >= 0) {
    message += "<input type='radio' name='disableSleepOnUSBPower' value='0'" + String(disableSleepOnUSBPower?"":" checked") + ">Sleep or disable WiFi on both USB or battery power<br>";
    message += "<input type='radio' name='disableSleepOnUSBPower' value='1'" + String(disableSleepOnUSBPower?" checked":"") + ">Only sleep or disable WiFi on battery power<br>";
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
  // If we are awaking from sleep we may want to disable WiFi
  //
  // NOTE: there is a bug in the revision 1 ESP32 hardware, which causes issues with wakeup from hibernate
  // to _not_ record this as a wakeup but in stead as an external reset (even though the info printed at
  // boot time is correct). For this reason it may be better to use deep sleep in stead of hibernate.
  // Various workarounds I've tried did not work.
  // See https://github.com/espressif/esp-idf/issues/494 for a description.
  //
  if (wifiActiveDuration > 0 && didWakeFromSleep) {
    IFDEBUG IotsaSerial.println("Disabling wifi");
    if (iotsaConfig.wifiEnabled) {
      IFDEBUG IotsaSerial.println("Wifi already enabled?");
    }
    iotsaConfig.disableWifiOnBoot = true;
  }
  if (watchdogDuration) {
    watchdogTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(watchdogTimer, &watchdogTimerTriggered, true);
    timerAlarmWrite(watchdogTimer, watchdogDuration*1000, false);
    timerAlarmEnable(watchdogTimer);
  }
#endif
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  static BLE2904 levelVBat2904;
  levelVBat2904.setFormat(BLE2904::FORMAT_UINT8);
  levelVBat2904.setUnit(0x27AD);
  static BLE2901 levelVBat2901("Battery Level");
  bleApi.addCharacteristic(levelVBatUUID, BLE_READ, &levelVBat2904, &levelVBat2901);
  static BLE2904 levelVUSB2904;
  levelVUSB2904.setFormat(BLE2904::FORMAT_UINT8);
  levelVUSB2904.setUnit(0x27AD);
  static BLE2901 levelVUSB2901("USB Voltage Level");
  bleApi.addCharacteristic(levelVUSBUUID, BLE_READ, &levelVUSB2904, &levelVUSB2901);
  static BLE2904 doSoftReboot2904;
  doSoftReboot2904.setFormat(BLE2904::FORMAT_BOOLEAN);
  doSoftReboot2904.setUnit(0x2700);
  static BLE2901 doSoftReboot2901("Reboot with WiFi");
  bleApi.addCharacteristic(doSoftRebootUUID, BLE_WRITE, &doSoftReboot2904, &doSoftReboot2901);
#endif
}

#ifdef IOTSA_WITH_API
bool IotsaBatteryMod::getHandler(const char *path, JsonObject& reply) {
  reply["sleepMode"] = (int)sleepMode;
  reply["sleepDuration"] = sleepDuration;
  reply["wakeDuration"] = wakeDuration;
  reply["bootExtraWakeDuration"] = bootExtraWakeDuration;
  reply["wifiActiveDuration"] = wifiActiveDuration;
#ifdef ESP32
  reply["watchdogDuration"] = watchdogDuration;
#endif
  _readVoltages();
  if (pinVBat >= 0) reply["levelVBat"] = levelVBat;
  if (pinVUSB >= 0) {
    reply["levelVUSB"] = levelVUSB;
    reply["disableSleepOnUSBPower"] = disableSleepOnUSBPower;
  }
  return true;
}

void IotsaBatteryMod::allowBLEConfigModeSwitch() {
  bleConfigModeSwitchAllowed = true;
  iotsaConfig.allowRCMDescription("use BLE to set 'reboot with WiFi' to 2");
}
bool IotsaBatteryMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("sleepMode")) {
    sleepMode = (IotsaSleepMode)reqObj["sleepMode"].as<int>();
    anyChanged = true;
  }
  if (reqObj.containsKey("sleepDuration")) {
    sleepDuration = reqObj["sleepDuration"];
    anyChanged = true;
  }
  if (reqObj.containsKey("wakeDuration")) {
    wakeDuration = reqObj["wakeDuration"];
    anyChanged = true;
  }
  if (reqObj.containsKey("bootExtraWakeDuration")) {
    bootExtraWakeDuration = reqObj["bootExtraWakeDuration"];
    anyChanged = true;
  }
  if (reqObj.containsKey("wifiActiveDuration")) {
    wifiActiveDuration = reqObj["wifiActiveDuration"];
    anyChanged = true;
  }
#ifdef ESP32
  if (reqObj.containsKey("wiatchdogDuration")) {
    watchdogDuration = reqObj["watchdogDuration"];
    anyChanged = true;
  }
#endif
  if (pinVUSB >= 0 && reqObj.containsKey("disableSleepOnUSBPower")) {
    disableSleepOnUSBPower = reqObj["disableSleepOnUSBPower"];
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
      IFDEBUG IotsaSerial.print("request reboot ");
      IFDEBUG IotsaSerial.println(doSoftReboot);
      return true;
  }
  return false;
}

bool IotsaBatteryMod::bleGetHandler(UUIDstring charUUID) {
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
  cf.get("wakeDuration", value, 0);
  wakeDuration = value;
  cf.get("bootExtraWakeDuration", value, 0);
  bootExtraWakeDuration = value;
  cf.get("sleepDuration", value, 0);
  sleepDuration = value;
  cf.get("wifiActiveDuration", value, 0);
  wifiActiveDuration = value;
#ifdef ESP32
  cf.get("watchdogDuration", value, 0);
  watchdogDuration = value;
#endif
  if (pinVUSB >= 0) {
    cf.get("disableSleepOnUSBPower", value, 0);
    disableSleepOnUSBPower = value;
  }
  millisAtWakeup = 0;
}

void IotsaBatteryMod::configSave() {
  IotsaConfigFileSave cf("/config/battery.cfg");
  cf.put("sleepMode", (int)sleepMode);
  cf.put("wakeDuration", (int)wakeDuration);
  cf.put("bootExtraWakeDuration", (int)bootExtraWakeDuration);
  cf.put("sleepDuration", (int)sleepDuration);
  cf.put("wifiActiveDuration", (int)wifiActiveDuration);
#ifdef ESP32
  cf.put("watchdogDuration", (int)watchdogDuration);
#endif
  if (pinVUSB >= 0) {
    cf.put("disableSleepOnUSBPower", (int)disableSleepOnUSBPower);
  }
  millisAtWakeup = 0;
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
  if (millisAtWifiWakeup == 0) millisAtWifiWakeup = millis();
  // If a reboot or configuration mode change has been requested (probably over BLE) we do so now.
  if (doSoftReboot) {
    if (doSoftReboot == 2 && bleConfigModeSwitchAllowed) {
      IFDEBUG IotsaSerial.println("Allow configmode change from BLE");
      iotsaConfig.allowRequestedConfigurationMode();
    } else {
      IFDEBUG IotsaSerial.println("Reboot from BLE");
      ESP.restart();
    }
    doSoftReboot = 0;
  }
  // Return quickly if no sleep or wifi sleep is required.
  if (sleepMode == IOTSA_SLEEP_NONE && wifiActiveDuration == 0) return;
  // Check whether we should disable Wifi or sleep
  int curWakeDuration = wakeDuration;
  if (!didWakeFromSleep) curWakeDuration += bootExtraWakeDuration;
  bool shouldDisableWifi = wifiActiveDuration && millis() > millisAtWifiWakeup + wifiActiveDuration;
  bool shouldSleep = sleepMode && curWakeDuration > 0 && millis() > millisAtWakeup + curWakeDuration;
  // Again, return quickly if no sleep or wifi sleep is required.
  if (!shouldSleep && !shouldDisableWifi) return;
  // Now check for other reasons NOT to go to sleep or disable wifi
  bool cancelSleep = pinDisableSleep >= 0 && digitalRead(pinDisableSleep) == LOW;
  // Another reason is if we're running on USB power and we only sleep on battery power
  if (disableSleepOnUSBPower && pinVUSB >= 0) {
    if (levelVUSB > 80) cancelSleep = true;
  }
  // Another reason is that we are in configuration mode
  if (iotsaConfig.inConfigurationMode()) cancelSleep = true;
  // A final reason is if some other module is asking for an extension of the waking period
  if (!iotsaConfig.canSleep()) cancelSleep = true;
  // If there is a reason not to sleep we return. We also reset the wake timer.
  if (cancelSleep) {
    IFDEBUG IotsaSerial.println("Sleep canceled");
    millisAtWakeup = millis();
    millisAtWifiWakeup = millis();
    return;
  }
  if (shouldDisableWifi) {
    if (iotsaConfig.wifiEnabled) {
      IotsaSerial.println("Disabling wifi");
      // Setting the iotsaConfig variables causes the wifi module to disable itself next loop()
      iotsaConfig.disableWifiOnBoot = true;
      iotsaConfig.wantWifiModeSwitch = true;
    }
  }
  if (!shouldSleep) return;
#ifdef ESP32
  if (watchdogTimer) timerAlarmDisable(watchdogTimer);
#endif
  // We go to sleep, in some form.
  IFDEBUG IotsaSerial.print("Going to sleep at ");
  IFDEBUG IotsaSerial.print(millis());
  IFDEBUG IotsaSerial.print(" for ");
  IFDEBUG IotsaSerial.print(sleepDuration);
  IFDEBUG IotsaSerial.print(" mode ");
  IFDEBUG IotsaSerial.println(sleepMode);

  if(sleepMode == IOTSA_SLEEP_DELAY) {
    // This isn't really sleeping, it's just a delay. Not sure it is actually useful.
    delay(sleepDuration);
    millisAtWakeup = 0;
#ifdef ESP32
    if (watchdogTimer) {
      timerWrite(watchdogTimer, 0);
      timerAlarmEnable(watchdogTimer);
    }
#endif
    return;
  }
#ifdef ESP32
  // We are going to sleep. First set the reasons for wakeup, such as a timer.
  // IFDEBUG delay(10); // Flush serial buffer
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
    IFDEBUG IotsaSerial.println(millisAtWakeup);
#ifdef IOTSA_WITH_BLE
    if (btActive) {
      IFDEBUG IotsaSerial.println("Re-activate ble");
      IotsaBLEServerMod::resumeServer();
    }
#endif
    if (watchdogTimer) {
      timerWrite(watchdogTimer, 0);
      timerAlarmEnable(watchdogTimer);
    }
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
  IFDEBUG IotsaSerial.println("esp_deep_sleep_start() failed?");
#else
  // For esp8266 only deep-sleep is implemented.
  ESP.deepSleep(sleepDuration*1000LL);
#endif
}

void IotsaBatteryMod::_readVoltages() {
  if (pinVBat >= 0) {
    int level = analogRead(pinVBat);
    // 3.9v input would give a reading of 4095 (at the default attenuation of 11dB). We scale, so a voltage of rangeVBat gives 100%
    levelVBat = int(100*3.9*level/(rangeVBat*4096));
//    IFDEBUG IotsaSerial.printf("analog level=%d levelVBat=%d\n", level, levelVBat);
    IFDEBUG IotsaSerial.print("VBat=");
    IFDEBUG IotsaSerial.println(levelVBat);
  }
  if (pinVUSB >= 0) {
    int level = analogRead(pinVUSB);
    levelVUSB = int(100*3.9*level/(rangeVUSB*4096));
//    IFDEBUG IotsaSerial.printf("analog level=%d levelVUSB=%d\n", level, levelVUSB);
  }
}