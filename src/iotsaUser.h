#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUserMod : public IotsaAuthMod {
public:
  IotsaUserMod(IotsaApplication &_app, const char *_username="admin", const char *_password="");
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override { return allows("api");}
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
protected:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Edit users and passwords"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  String username;
  String password;
};

#endif
