#ifndef _IOTSABATTERY_H_
#define _IOTSABATTERY_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

#ifdef IOTSA_WITH_API
#define IotsaBatteryModBaseMod IotsaApiMod
#else
#define IotsaBatteryModBaseMod IotsaMod
#endif

enum IotsaSleepMode : uint8_t {
  IOTSA_SLEEP_NONE,
  IOTSA_SLEEP_DELAY,
  IOTSA_SLEEP_LIGHT,
  IOTSA_SLEEP_DEEP,
  IOTSA_SLEEP_HIBERNATE,
  _IOTSA_SLEEP_MAX
};

class IotsaBatteryMod : public IotsaBatteryModBaseMod, public IotsaBLEApiProvider {
public:
  IotsaBatteryMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaBatteryModBaseMod(_app, _auth, true) {}

  void setup();
  void serverSetup();
  void loop();
  String info();
  void setPinVUSB(int pin, float range=2.5) { pinVUSB = pin; rangeVUSB = range; }
  void setPinVBat(int pin, float range=1.8) { pinVBat = pin; rangeVBat = range; }
  void setPinDisableSleep(int pin) { pinDisableSleep = pin; }
  void allowBLEConfigModeSwitch();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  String argument;
  enum IotsaSleepMode sleepMode;
  bool didWakeFromSleep = false;
  int doSoftReboot = 0;
  bool bleConfigModeSwitchAllowed = false;
  uint32_t millisAtWakeup = 0;
  uint32_t millisAtWifiWakeup = 0;
  uint32_t wakeDuration = 0;
  uint32_t bootExtraWakeDuration = 0;
  uint32_t sleepDuration = 0;
  uint32_t wifiActiveDuration = 0;
  uint8_t disableSleepOnUSBPower = 0;
  void _readVoltages();
  int pinVBat = -1;
  int pinVUSB = -1;
  float rangeVBat = 2.5;
  float rangeVUSB = 1.8;
  int pinDisableSleep = -1;
  uint8_t levelVBat;
  uint8_t levelVUSB;
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID);
  bool bleGetHandler(UUIDstring charUUID);
  static constexpr UUIDstring serviceUUID = "180F";
  static constexpr UUIDstring levelVBatUUID = "2A19";
  static constexpr UUIDstring levelVUSBUUID = "E4D90002-250F-46E6-90A4-AB98F01A0587";
  static constexpr UUIDstring doSoftRebootUUID = "E4D90003-250F-46E6-90A4-AB98F01A0587";
#endif // IOTSA_WITH_BLE
};

#endif
