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
  SLEEP_NONE,
  SLEEP_DELAY,
  SLEEP_LIGHT,
  SLEEP_DEEP,
  SLEEP_HIBERNATE,
  SLEEP_DEEP_NOWIFI,
  SLEEP_HIBERNATE_NOWIFI,
  SLEEP_ADAPTIVE_NOWIFI
};

class IotsaBatteryMod : public IotsaBatteryModBaseMod, public IotsaBLEApiProvider {
public:
  IotsaBatteryMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL) : IotsaBatteryModBaseMod(_app, _auth, true) {}

  void setup();
  void serverSetup();
  void loop();
  String info();
  void setPinVUSB(int pin) { pinVUSB = pin; }
  void setPinVBat(int pin) { pinVBat = pin; }
  void setPinDisableSleep(int pin) { pinDisableSleep = pin; }
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  String argument;
  enum IotsaSleepMode sleepMode;
  bool didWakeFromSleep = false;
  bool doSoftReboot = false;
  uint32_t millisAtWakeup = 0;
  uint32_t wakeDuration = 0;
  uint32_t bootExtraWakeDuration = 0;
  uint32_t sleepDuration = 0;
  uint8_t disableSleepOnUSBPower = 0;
  void _readVoltages();
  int pinVBat = -1;
  int pinVUSB = -1;
  int pinDisableSleep = -1;
  uint8_t levelVBat;
  uint8_t levelVUSB;
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID);
  bool bleGetHandler(UUIDstring charUUID);
  static constexpr UUIDstring serviceUUID = "CCEC1777-3F1D-435F-957D-6789F49FFEB8";
  static constexpr UUIDstring levelVBatUUID = "94482666-A8D0-4585-B22F-75C0C40A272F";
  static constexpr UUIDstring levelVUSBUUID = "E4D98D37-250F-46E6-90A4-AB98F01A0587";
  static constexpr UUIDstring doSoftRebootUUID = "21A2434E-44FA-4B7A-8E17-88676AF9DA0F";
#endif // IOTSA_WITH_BLE
};

#endif
