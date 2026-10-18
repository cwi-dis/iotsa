#ifndef _IOTSANOTHING_H_
#define _IOTSANOTHING_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaNothingMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Boilerplate module"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  String argument;
};

#endif
