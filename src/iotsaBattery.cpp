#include "iotsa.h"
#include "iotsaBattery.h"
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"
#include "iotsaRunmode.h"   // setPinDisableSleep() / allowBLEConfigModeSwitch() forward here (cwi-dis/iotsa#106)

// IotsaBatteryMod is battery *hardware* only now: VBat / VUSB ADC sensing and the
// 180F BLE service. Sleep/wake moved to IotsaSleepPolicy + IotsaRunmodeMod
// (cwi-dis/iotsa#106). The doSoftReboot BLE gesture still lives here.

// How often loop() re-samples the ADCs so iotsaStatus.onUsbPower (read by the
// sleep policy) doesn't go stale between REST/BLE queries.
static const uint32_t VOLTAGE_READ_INTERVAL_MS = 5000;

void IotsaBatteryMod::setup() {
  configLoad();
}

void IotsaBatteryMod::lateSetup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  bleApi.addCharacteristic(levelVBatUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x27AD, "Battery Level");
  bleApi.addCharacteristic(levelVUSBUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT8, 0x27AD, "USB Voltage Level");
#endif
  api.setup("battery", true, true);
  name = "battery";
  // The voltage calibration is a property of this device's hardware.
  changesNeedRight("config");
}

void IotsaBatteryMod::loop() {
  if (_lastVoltageReadMillis == 0 || millis() - _lastVoltageReadMillis >= VOLTAGE_READ_INTERVAL_MS) {
    _lastVoltageReadMillis = millis();
    _readVoltages();
  }
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

void IotsaBatteryMod::putHandler(IotsaApiContext& ctx) {
  if (pinVBat >= 0) ctx.arg("correctionVBat", correctionVBat, IotsaApiContext::Saved);
}

void IotsaBatteryMod::getHandler(IotsaApiContext& ctx) {
  _readVoltages();
  if (pinVBat >= 0) {
    ctx.show("levelVBat", levelVBat);
    ctx.show("correctionVBat", correctionVBat);
  }
  if (pinVUSB >= 0) {
    ctx.show("levelVUSB", levelVUSB);
    ctx.show("onUsbPower", iotsaStatus.onUsbPower);
  }
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

#ifdef IOTSA_WITH_WEB
void IotsaBatteryMod::formHandler_args(IotsaWebContext& ctx) {
  if (pinVBat >= 0) ctx.arg("correctionVBat", correctionVBat, IotsaWebContext::Saved);
}

void IotsaBatteryMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    _readVoltages();
    if (pinVBat >= 0) ctx.showStatus("Battery level", "levelVBat", String(levelVBat) + "%");
    if (pinVUSB >= 0) ctx.showStatus("USB voltage level", "levelVUSB", String(levelVUSB) + "%");
  } else if (pinVBat >= 0) {
    IotsaWebContext::Form form(ctx);
    ctx.showField("Battery voltage correction factor", "correctionVBat", correctionVBat, IotsaWebContext::Saved);
  }
}

String IotsaBatteryMod::info() {
  String message = "<p>Built with battery module. See <a href=\"/battery\">/battery</a>.";
  message += " Or access the REST interface at <a href='/api/battery'>/api/battery</a>.";
#ifdef IOTSA_WITH_BLE
  message += " Or use BLE service " + String(serviceUUID) + " on device " + iotsaConfig.hostName + ".";
#endif
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB
