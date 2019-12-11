#ifndef _IOTSABATTERY_H_
#define _IOTSABATTERY_H_
#include "iotsa.h"
#include "iotsaApi.h"

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
  SLEEP_HIBERNATE
};

class IotsaBatteryMod : public IotsaBatteryModBaseMod {
public:
  using IotsaBatteryModBaseMod::IotsaBatteryModBaseMod;
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
  uint32_t millisAtWakeup = 0;
  uint32_t wakeDuration = 0;
  uint32_t sleepDuration = 0;
  void _readVoltages();
  int pinVBat = -1;
  int pinVUSB = -1;
  int pinDisableSleep = -1;
  uint8_t levelVBat;
  uint8_t levelVUSB;
};

#endif
