#ifndef _IOTSAHELLOMOD_H_
#define _IOTSAHELLOMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
// Hello "name" module. Greets visitors to the /hello page, and allows them
// to change the name by which they are greeted, through the web UI and/or
// the REST API -- both gated on rights-based access control (see
// IotsaMultiUserMod).
//
class IotsaHelloMod : public IotsaModule {
public:
  IotsaHelloMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL) : IotsaModule(_app, _auth, false) {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  using IotsaBaseModule::needsAuthentication;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
private:
  void webHandler() override;
  String greeting;
};

#endif
