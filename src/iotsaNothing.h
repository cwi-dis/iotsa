#ifndef _IOTSANOTHING_H_
#define _IOTSANOTHING_H_
#include "iotsa.h"
#include "iotsaApi.h"

//
// A test fixture: a module that does nothing, with one setting ("argument"),
// a web page /nothing and GET/PUT /api/nothing. sandbox/Infra uses it as its
// one trivial application module, tests/KitchenSink as the target of its
// IotsaRequest loopback. Not a template: start your own module from
// examples/HelloIotsa.
//
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
  void configLoad() override;
  void configSave() override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Boilerplate module"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  String argument;
};

#endif
