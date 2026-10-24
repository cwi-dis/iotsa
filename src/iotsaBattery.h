#ifndef _IOTSABATTERY_H_
#define _IOTSABATTERY_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

// IotsaBatteryMod shrank to battery *hardware* -- VBat/VUSB ADC sensing + the
// 180F BLE battery service (cwi-dis/iotsa#106). The sleep/wake config + decision
// moved to IotsaController's IotsaSleepPolicy and the executor to IotsaRunmodeMod
// (which owns IOTSA_HAS_SLEEP). This module publishes iotsaStatus.onUsbPower for
// the sleep policy to read. pinDisableSleep is IotsaRunmodeMod's
// (setPinDisableSleep()), the BLE mode-promote gesture the runmode service's,
// allowed by the BLE server's promoteAllowed setting.

class IotsaBatteryMod : public IotsaModule {
public:
  IotsaBatteryMod(IotsaApplication &_app) : IotsaModule(_app, true) {}

  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  void setPinVUSB(int pin, float range=2.5) { pinVUSB = pin; rangeVUSB = range; }
  void setPinVBat(int pin, float range=3.6, float minRange=0) { pinVBat = pin; rangeVBat = range; rangeVBatMin = minRange; }
protected:
  void configLoad() override;
  void configSave() override;
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Battery module"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
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
