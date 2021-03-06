#ifndef _IOTSANOTHING_H_
#define _IOTSANOTHING_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_API
#define IotsaNothingModBaseMod IotsaApiMod
#else
#define IotsaNothingModBaseMod IotsaMod
#endif

class IotsaNothingMod : public IotsaNothingModBaseMod {
public:
  using IotsaNothingModBaseMod::IotsaNothingModBaseMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  String argument;
};

#endif
