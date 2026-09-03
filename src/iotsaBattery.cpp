#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"
#include "iotsaRunmode.h"   // setPinDisableSleep() / allowBLEConfigModeSwitch() forward here (cwi-dis/iotsa#106)

// IotsaBatteryMod is battery *hardware* only now: VBat / VUSB ADC sensing and the
// 180F BLE service. Sleep/wake moved to IotsaSleepPolicy + IotsaRunmodeMod
// (cwi-dis/iotsa#106). The doSoftReboot BLE gesture still lives here.

#ifdef IOTSA_WITH_WEB
void
IotsaBatteryMod::webHandler() {
  bool anyChanged = false;
  if (api.webService->server->hasArg("correctionVBat")) {
    if (needsAuthentication()) return;
    correctionVBat = api.webService->server->arg("correctionVBat").toFloat();
    anyChanged = true;
  }
  if (anyChanged) {
    iotsaController.extendCurrentMode();
    configSave();
  }

  String message = "<html><head><title>Battery module</title></head><body><h1>Battery module</h1>";
  _readVoltages();
  message += "<p>";
  if (pinVBat >= 0) {
    message += "Battery level: " + String(levelVBat) + "%<br>";
  }
  if (pinVUSB >= 0) {
    message += "USB voltage level: " + String(levelVUSB) + "%<br>";
  }
  message += "</p>";
  message += "<form method='post'>";
  if (pinVBat >= 0) {
    message += "Battery voltage correction factor: <input name='correctionVBat' value='" + String(correctionVBat) + "'><br>";
  }
  message += "<input type='submit'></form>";
#ifdef IOTSA_HAS_SLEEP
  message += "<p>Sleep/wake settings moved to <a href=\"/runmode\">/runmode</a>.</p>";
#endif
  api.webService->server->send(200, "text/html", message);
}

String IotsaBatteryMod::info() {
  String message = "<p>Built with battery module. See <a href=\"/battery\">/battery</a>.";
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
}

void IotsaBatteryMod::setPinDisableSleep(int pin) {
#ifdef IOTSA_HAS_SLEEP
  if (IotsaRunmodeMod::instance()) IotsaRunmodeMod::instance()->setPinDisableSleep(pin);
#else
  (void)pin;
  IFDEBUG IotsaSerial.println("iotsaBattery: setPinDisableSleep ignored, built without IOTSA_HAS_SLEEP");
#endif
}

void IotsaBatteryMod::allowBLEConfigModeSwitch() {
  // The BLE mode-promote gesture moved to IotsaRunmodeMod's control service
  // (cwi-dis/iotsa#106). Transitional forwarder for the ~4 downstream callers.
#ifdef IOTSA_WITH_BLE
  if (IotsaRunmodeMod::instance()) IotsaRunmodeMod::instance()->allowBLEModeSwitch();
#endif
}

bool IotsaBatteryMod::getHandler(const char *path, JsonObject& reply) {
  _readVoltages();
  if (pinVBat >= 0) {
    reply["levelVBat"] = levelVBat;
    reply["correctionVBat"] = correctionVBat;
  }
  if (pinVUSB >= 0) {
    reply["levelVUSB"] = levelVUSB;
    reply["onUsbPower"] = iotsaStatus.onUsbPower;
  }
  return true;
}

bool IotsaBatteryMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (pinVBat >= 0 && reqObj["correctionVBat"].is<float>()) {
    correctionVBat = reqObj["correctionVBat"];
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}

#ifdef IOTSA_WITH_BLE
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

void IotsaBatteryMod::lateSetup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  bleApi.addCharacteristic(levelVBatUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x27AD, "Battery Level");
  bleApi.addCharacteristic(levelVUSBUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x27AD, "USB Voltage Level");
#endif
  api.setup("battery", true, true);
  name = "battery";
}

void IotsaBatteryMod::configLoad() {
  IotsaConfigFileLoad cf("/config/battery.cfg");
  cf.get("correctionVBat", correctionVBat, 1.0);
}

void IotsaBatteryMod::configSave() {
  IotsaConfigFileSave cf("/config/battery.cfg");
  if (pinVBat >= 0) {
    cf.put("correctionVBat", correctionVBat);
  }
}

void IotsaBatteryMod::loop() {
  static bool firstLoop = true;
  if (firstLoop) {
    firstLoop = false;
    _readVoltages();
  }
}

void IotsaBatteryMod::_readVoltages() {
  if (pinVBat >= 0) {
    int level = analogRead(pinVBat);
    // 3.9v input would give a reading of 4095 (at the default attenuation of 11dB). We scale, so a voltage of rangeVBat gives 100%
    // See https://esphome.io/components/sensor/adc.html#adc-esp32-attenuation for a description of the attenuation
    float lvbFloat = (level * correctionVBat * 3.9)/ 4096.0;
    float charge = (lvbFloat - rangeVBatMin) / (rangeVBat - rangeVBatMin);
    levelVBat = charge <= 0 ? 0 : int(100*charge);
    IFDEBUG IotsaSerial.print("VBat=");
    IFDEBUG IotsaSerial.println(levelVBat);
  }
  if (pinVUSB >= 0) {
    int level = analogRead(pinVUSB);
    levelVUSB = int(100*3.9*level/(rangeVUSB*4096));
  }
  // Publish "on USB power" for IotsaSleepPolicy (cwi-dis/iotsa#106). No VUSB
  // sense => assume battery, so sleep is not USB-gated.
  iotsaStatus.onUsbPower = (pinVUSB >= 0 && levelVUSB > 80);
}
