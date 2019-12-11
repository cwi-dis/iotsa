#ifndef _IOTSABATTERY_H_
#define _IOTSABATTERY_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_API
#define IotsaBatteryModBaseMod IotsaApiMod
#else
#define IotsaBatteryModBaseMod IotsaMod
#endif

class IotsaBatteryMod : public IotsaBatteryModBaseMod {
public:
  using IotsaBatteryModBaseMod::IotsaBatteryModBaseMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
  void setPinVUSB(int pin) { pinVUSB = pin; }
  void setPinVBat(int pin) { pinVBat = pin; }
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  String argument;
  uint32_t millisAtBoot = 0;
  uint32_t wakeDuration = 0;
  uint32_t sleepDuration = 0;
  void _readVoltages();
  int pinVBat = 0;
  int pinVUSB = 0;
  int16_t levelVBat;
  int16_t levelVUSB;
};

#endif
