#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUserMod : public IotsaAuthMod, public IotsaApiProvider {
public:
  IotsaUserMod(IotsaApplication &_app, const char *_username="admin", const char *_password="");
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool allows(const char *right=NULL);
  bool allows(const char *obj, const char *verb) { return allows("api");}
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply);
protected:
  void configLoad();
  void configSave();
  void handler();
  String username;
  String password;
  IotsaApi api;
};

#endif
