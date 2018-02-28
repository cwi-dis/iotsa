#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUserMod : public IotsaAuthApiMod {
public:
  IotsaUserMod(IotsaApplication &_app, const char *_username="admin", const char *_password="");
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool needsAuthentication(const char *right=NULL);
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();
  String username;
  String password;
};

#endif
