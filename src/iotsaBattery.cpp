#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"
#ifdef ESP32
#include <esp_wifi.h>
#include <esp_bt.h>
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
    if (useExtraWakeDuration) nextSleepTime += bootExtraWakeDuration;
    message += "Remaining awake for: " + String((nextSleepTime - millis())/1000.0) + "s<br>";
  }
  if (pinVBat >= 0) {
    message += "Battery level: " + String(levelVBat) + "%<br>";
  }
  if (pinVUSB >= 0) {
    message += "USB voltage level: " + String(levelVUSB) + "%<br>";
  }
  message += "</p>";
  message += "<form method='get'>";
  message += "Sleep mode: <input name='sleepMode' value='" + String(sleepMode) + "'><br>";
  message += "Sleep duration: <input name='sleepDuration' value='" + String(sleepDuration) + "'><br>";
  message += "Wake duration: <input name='wakeDuration' value='" + String(wakeDuration) + "'><br>";
  message += "Extra wake duration after poweron/reset: <input name='bootExtraWakeDuration' value='" + String(bootExtraWakeDuration) + "'><br>";
  if (pinVUSB >= 0) {
    message += "<input type='radio' name='disableSleepOnUSBPower' value='0'" + String(disableSleepOnUSBPower?"":" checked") + ">Sleep on USB or battery power<br>";
    message += "<input type='radio' name='disableSleepOnUSBPower' value='1'" + String(disableSleepOnUSBPower?" checked":"") + ">Only sleep on battery power<br>";
  }
  message += "<input type='submit'></form>";
  server->send(200, "text/html", message);
}

String IotsaBatteryMod::info() {
  String message = "<p>Built with battery module. See <a href=\"/battery\">/battery</a> to change the battery power saving options.";
#ifdef IOTSA_WITH_API
  message += " Or access the REST interface at <a href='/api/battery'>/api/battery.";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaBatteryMod::setup() {
  configLoad();
#ifdef ESP32
  useExtraWakeDuration = (esp_sleep_get_wakeup_cause() == 0);
  // If we are awaking from sleep we may want top disable WiFi
  //
  // NOTE: there is a bug in the revision 1 ESP32 hardware, which causes issues with wakeup from hibernate
  // to _not_ record this as a wakeup but in stead as an external reset (even though the info printed at
  // boot time is correct). For this reason it may be better to use deep sleep in stead of hibernate.
  // Various workarounds I've tried did not work.
  // See https://github.com/espressif/esp-idf/issues/494 for a description.
  //
  if ((sleepMode == SLEEP_HIBERNATE_NOWIFI || sleepMode == SLEEP_DEEP_NOWIFI) && !useExtraWakeDuration) {
    IFDEBUG IotsaSerial.println("Disabling wifi");
    if (iotsaConfig.wifiEnabled) {
      IFDEBUG IotsaSerial.println("Wifi already enabled?");
    }
    iotsaConfig.disableWifiOnBoot = true;
  }
#endif
}

#ifdef IOTSA_WITH_API
bool IotsaBatteryMod::getHandler(const char *path, JsonObject& reply) {
  reply["sleepMode"] = (int)sleepMode;
  reply["sleepDuration"] = sleepDuration;
  reply["wakeDuration"] = wakeDuration;
  reply["bootExtraWakeDuration"] = bootExtraWakeDuration;
  _readVoltages();
  if (pinVBat >= 0) reply["levelVBat"] = levelVBat;
  if (pinVUSB >= 0) {
    reply["levelVUSB"] = levelVUSB;
    reply["disableSleepOnUSBPower"] = disableSleepOnUSBPower;
  }
  return true;
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
  if (pinVUSB >= 0 && reqObj.containsKey("disableSleepOnUSBPower")) {
    disableSleepOnUSBPower = reqObj["disableSleepOnUSBPower"];
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaBatteryMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/battery", std::bind(&IotsaBatteryMod::handler, this));
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
  sleepMode = (IotsaSleepMode)value;
  cf.get("wakeDuration", value, 0);
  wakeDuration = value;
  cf.get("bootExtraWakeDuration", value, 0);
  bootExtraWakeDuration = value;
  cf.get("sleepDuration", value, 0);
  sleepDuration = value;
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
  if (pinVUSB >= 0) {
    cf.put("disableSleepOnUSBPower", (int)disableSleepOnUSBPower);
  }
  millisAtWakeup = 0;
}

void IotsaBatteryMod::loop() {
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
  int curWakeDuration = wakeDuration;
  if (useExtraWakeDuration) curWakeDuration += bootExtraWakeDuration;
  if (sleepMode && curWakeDuration > 0 && millis() > millisAtWakeup + curWakeDuration) {
    bool cancelSleep = pinDisableSleep >= 0 && digitalRead(pinDisableSleep) == LOW;
    if (disableSleepOnUSBPower && pinVUSB >= 0) {
      if (levelVUSB > 80) cancelSleep = true;
    }
    if (cancelSleep) {
      IFDEBUG IotsaSerial.println("Sleep canceled");
      millisAtWakeup = millis();
      return;
    }
    IFDEBUG IotsaSerial.print("Going to sleep at ");
    IFDEBUG IotsaSerial.print(millis());
    IFDEBUG IotsaSerial.print(" for ");
    IFDEBUG IotsaSerial.print(sleepDuration);
    IFDEBUG IotsaSerial.print(" mode ");
    IFDEBUG IotsaSerial.println(sleepMode);
    if(sleepMode == SLEEP_DELAY) {
      delay(sleepDuration);
      millisAtWakeup = 0;
    } else {
#ifdef ESP32
      IFDEBUG delay(10); // Flush serial buffer
      if (sleepDuration) {
        esp_sleep_enable_timer_wakeup(sleepDuration*1000LL);
      } else {
        // xxxjack configure other wakeup sources...
      }
      if (sleepMode == SLEEP_LIGHT) {
        esp_light_sleep_start();
        IFDEBUG IotsaSerial.print("light sleep wakup at ");
        millisAtWakeup = millis();
        IFDEBUG IotsaSerial.println(millisAtWakeup);
        return;
      }
      if (iotsaConfig.wifiEnabled) esp_wifi_stop();
      esp_bt_controller_disable();
      if (sleepMode == SLEEP_HIBERNATE || sleepMode == SLEEP_HIBERNATE_NOWIFI) {
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
      }
      esp_deep_sleep_start();
      IFDEBUG IotsaSerial.println("esp_*_sleep_start() failed?");
#else
      ESP.deepSleep(sleepDuration*1000LL);
#endif
    }
  }
}

void IotsaBatteryMod::_readVoltages() {
    if (pinVBat >= 0) {
    int level = analogRead(pinVBat);
    levelVBat = int(100*3.6*level/(2.0*4096));
    IFDEBUG IotsaSerial.print("VBat=");
    IFDEBUG IotsaSerial.println(levelVBat);
  }
  if (pinVUSB >= 0) {
    int level = analogRead(pinVUSB);
    levelVUSB = int(100*3.3*level/(2.0*4096));
    IFDEBUG IotsaSerial.print("VUSB=");
    IFDEBUG IotsaSerial.println(levelVUSB);
  }
}