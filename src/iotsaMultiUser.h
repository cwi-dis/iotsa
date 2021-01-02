#ifndef _IOTSAUSERS_H_
#define _IOTSAUSERS_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUser : IotsaApiModObject {
public:
  IotsaUser() {}
  String username;
  String password;
  String rights;
  String apiEndpoint;
public:
  bool configLoad(IotsaConfigFileLoad& cf, String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, String& f_name) override;
#ifdef IOTSA_WITH_WEB
  static void formHandler(String& message) /*override*/;
  static void formHandlerTH(String& message) /*override*/;
  void formHandler(String& message, String& text, String& f_name) override;
  void formHandlerTD(String& message) override;
  bool formArgHandler(IotsaWebServer *server, String name) override;
#endif
#ifdef IOTSA_WITH_API
  void getHandler(JsonObject& reply) override;
  bool putHandler(const JsonVariant& request) override;
#endif
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
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
protected:
  void configLoad();
  void configSave();
  void handler();
  std::vector<IotsaUser> users;
  int _addUser(IotsaUser& newUser);
  // _delUser(int) not implemented, because of /api/users/<num> which would change
#ifdef IOTSA_WITH_API
  IotsaApiService api;
#endif
};

#endif
