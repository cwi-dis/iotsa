#ifndef _IOTSANOTHING_H_
#define _IOTSANOTHING_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaNothingMod : public IotsaApiMod {
public:
  using IotsaApiMod::IotsaApiMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
  void handler();
  String argument;
};

#endif
