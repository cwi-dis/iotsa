#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"

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
  if (anyChanged) configSave();

  String message = "<html><head><title>Battery power saving module</title></head><body><h1>Battery power saving module</h1>";
  _readVoltages();
  message += "<p>Wakeup time: " + String(millisAtWakeup) + "ms<br>";
  message += "Awake for: " + String(millis() - millisAtWakeup) + "ms<br>";
  if (sleepMode && wakeDuration) {
    message += "Remaining awake for: " + String(millisAtWakeup + wakeDuration - millis()) + "ms<br>";
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
}

#ifdef IOTSA_WITH_API
bool IotsaBatteryMod::getHandler(const char *path, JsonObject& reply) {
  reply["sleepMode"] = (int)sleepMode;
  reply["sleepDuration"] = sleepDuration;
  reply["wakeDuration"] = wakeDuration;
  _readVoltages();
  if (pinVBat >= 0) reply["levelVBat"] = levelVBat;
  if (pinVUSB >= 0) reply["levelVUSB"] = levelVUSB;
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
  cf.get("sleepDuration", value, 0);
  sleepDuration = value;
  millisAtWakeup = 0;
}

void IotsaBatteryMod::configSave() {
  IotsaConfigFileSave cf("/config/battery.cfg");
  cf.put("sleepMode", (int)sleepMode);
  cf.put("wakeDuration", (int)wakeDuration);
  cf.put("sleepDuration", (int)sleepDuration);
  millisAtWakeup = 0;
}

void IotsaBatteryMod::loop() {
  if (millisAtWakeup == 0) {
    millisAtWakeup = millis();
    IFDEBUG IotsaSerial.print("wakeup at ");
    IFDEBUG IotsaSerial.print(millisAtWakeup);
    IFDEBUG IotsaSerial.print(" reason ");
    IFDEBUG IotsaSerial.println(esp_sleep_get_wakeup_cause());
    _readVoltages();
  }
  if (sleepMode && wakeDuration > 0 && millis() > millisAtWakeup + wakeDuration) {
    if (pinDisableSleep >= 0 && digitalRead(pinDisableSleep) == LOW) {
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
      } else
      if (sleepMode == SLEEP_DEEP) {
        esp_deep_sleep_start();
      } else
      if (sleepMode == SLEEP_HIBERNATE) {
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
        esp_deep_sleep_start();
      }
      IFDEBUG IotsaSerial.println("esp_*_sleep_start() failed?");
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