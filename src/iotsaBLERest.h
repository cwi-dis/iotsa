#ifndef _IOTSABLEREST_H_
#define _IOTSABLEREST_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_API
#define IotsaBLERestModBaseMod IotsaApiMod
#else
#define IotsaBLERestModBaseMod IotsaMod
#endif

class IotsaBLERestMod : public IotsaBLERestModBaseMod {
public:
  using IotsaBLERestModBaseMod::IotsaBLERestModBaseMod;
  void setup() override;
  void loop() override;
  void serverSetup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
};

#endif
