#ifndef _IOTSABATTERY_H_
#define _IOTSABATTERY_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

// IotsaBatteryMod shrank to battery *hardware* -- VBat/VUSB ADC sensing + the
// 180F BLE battery service (cwi-dis/iotsa#106). The sleep/wake config + decision
// moved to IotsaController's IotsaSleepPolicy and the executor to IotsaRunmodeMod
// (which owns IOTSA_HAS_SLEEP). This module publishes iotsaStatus.onUsbPower for
// the sleep policy to read. The doSoftReboot / allowBLEConfigModeSwitch BLE
// gesture still lives here for now.

class IotsaBatteryMod : public IotsaModule {
public:
  IotsaBatteryMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaModule(_app, _auth, true) {}

  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  void setPinVUSB(int pin, float range=2.5) { pinVUSB = pin; rangeVUSB = range; }
  void setPinVBat(int pin, float range=3.6, float minRange=0) { pinVBat = pin; rangeVBat = range; rangeVBatMin = minRange; }
  // Transitional forwarders to IotsaRunmodeMod, now the home for pinDisableSleep
  // and the BLE mode-promote gesture (cwi-dis/iotsa#106). Kept so the handful of
  // downstream callers keep compiling until the #106 sweep.
  void setPinDisableSleep(int pin);
  void allowBLEConfigModeSwitch();
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
  void _readVoltages();
  uint32_t _lastVoltageReadMillis = 0;   // loop() re-reads periodically so iotsaStatus.onUsbPower stays fresh
  int pinVBat = -1;
  int pinVUSB = -1;
  float rangeVBat = 3.3;
  float rangeVBatMin = 0;
  float correctionVBat = 1.0;
  float rangeVUSB = 1.8;
  uint8_t levelVBat;
  uint8_t levelVUSB;
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "180F";
  static constexpr UUIDstring levelVBatUUID = "2A19";
  static constexpr UUIDstring levelVUSBUUID = "E4D90002-250F-46E6-90A4-AB98F01A0587";
#endif // IOTSA_WITH_BLE
};

#endif
