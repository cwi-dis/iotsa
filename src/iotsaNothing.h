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
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
  void configLoad() override;
  void configSave() override;
  void handler();
  String argument;
};

#endif
