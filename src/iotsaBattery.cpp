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
  if( server->hasArg("wakeDuration")) {
    if (needsAuthentication()) return;
    wakeDuration = server->arg("wakeDuration").toInt();
    anyChanged = true;
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Battery power saving module</title></head><body><h1>Battery power saving module</h1>";
  _readVoltages();
  if (pinVBat > 0) {
    message += "<p>Battery level: " + String(levelVBat) + "</p>";
  }
  if (pinVUSB > 0) {
    message += "<p>USB voltage level: " + String(levelVUSB) + "</p>";
  }
  message += "<form method='get'>";
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
  reply["sleepDuration"] = sleepDuration;
  reply["wakeDuration"] = wakeDuration;
  _readVoltages();
  if (pinVBat > 0) reply["levelVBat"] = levelVBat;
  if (pinVUSB > 0) reply["levelVUSB"] = levelVUSB;
  return true;
}

bool IotsaBatteryMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
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
  cf.get("wakeDuration", value, 0);
  wakeDuration = value;
  cf.get("sleepDuration", value, 0);
  sleepDuration = value;
 
}

void IotsaBatteryMod::configSave() {
  IotsaConfigFileSave cf("/config/battery.cfg");
  cf.put("wakeDuration", (int)wakeDuration);
  cf.put("sleepDuration", (int)sleepDuration);
}

void IotsaBatteryMod::loop() {
  if (millisAtBoot == 0) {
    millisAtBoot = millis();
    IFDEBUG IotsaSerial.print("wakeup at ");
    IFDEBUG IotsaSerial.println(millisAtBoot);
    _readVoltages();
  }
  if (wakeDuration > 0 && millis() > millisAtBoot + wakeDuration) {
    IFDEBUG IotsaSerial.print("Going to sleep at ");
    IFDEBUG IotsaSerial.print(millis());
    IFDEBUG IotsaSerial.print(" for ");
    IFDEBUG IotsaSerial.println(sleepDuration);
    IFDEBUG delay(10);
    delay(sleepDuration);
    millisAtBoot = 0;
  }
}

void IotsaBatteryMod::_readVoltages() {
    if (pinVBat > 0) {
    int level = analogRead(pinVBat);
    levelVBat = int(100*3.6*level/(2.0*4096));
    IFDEBUG IotsaSerial.print("VBat=");
    IFDEBUG IotsaSerial.println(levelVBat);
  }
  if (pinVUSB > 0) {
    int level = analogRead(pinVUSB);
    levelVUSB = int(100*3.3*level/(2.0*4096));
    IFDEBUG IotsaSerial.print("VUSB=");
    IFDEBUG IotsaSerial.println(levelVUSB);
  }
}