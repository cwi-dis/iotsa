#ifndef _IOTSAUSERS_H_
#define _IOTSAUSERS_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUser {
public:
  IotsaUser() : username(""), password(""), rights(""), apiEndpoint(""), next(NULL) {}
  String username;
  String password;
  String rights;
  String apiEndpoint;
  IotsaUser *next;
};

class IotsaMultiUserMod : public IotsaAuthMod, public IotsaApiProvider {
public:
  IotsaMultiUserMod(IotsaApplication &_app);
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool allows(const char *right=NULL);
  bool allows(const char *obj, IotsaApiOperation verb);
  bool getHandler(const char *path, JsonObject& reply);
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
protected:
  void configLoad();
  void configSave();
  void handler();
  IotsaUser *users;
#ifdef IOTSA_WITH_API
  IotsaApiService api;
#endif
};

#endif
