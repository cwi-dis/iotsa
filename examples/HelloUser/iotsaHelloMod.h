#ifndef _IOTSAHELLOMOD_H_
#define _IOTSAHELLOMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
// Hello "name" module. Greets visitors to the /hello page, and allows them
// to change the name by which they are greeted, through the web UI and/or
// the REST API -- both gated on the user's rights (see IotsaUserMod).
//
class IotsaHelloMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Hello Server"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  String personName = "human";
};

#endif
