#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUserMod : public IotsaAuthMod, public IotsaApiProvider {
public:
  IotsaUserMod(IotsaApplication &_app, const char *_username="admin", const char *_password="");
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override { return allows("api");}
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
protected:
  void configLoad() override;
  void configSave() override;
  void handler();
  String username;
  String password;
#ifdef IOTSA_WITH_API
  IotsaApiService api;
#endif
};

#endif
