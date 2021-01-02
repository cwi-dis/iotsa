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
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
  bool getHandler(const char *path, JsonObject& reply) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
protected:
  void configLoad() override;
  void configSave() override;
  void handler();
  std::vector<IotsaUser> users;
  int _addUser(IotsaUser& newUser);
  // _delUser(int) not implemented, because of /api/users/<num> which would change
#ifdef IOTSA_WITH_API
  IotsaApiService api;
#endif
};

#endif
