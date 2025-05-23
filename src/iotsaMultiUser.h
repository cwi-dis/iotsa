#ifndef _IOTSAUSERS_H_
#define _IOTSAUSERS_H_
#include <vector>
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
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
#ifdef IOTSA_WITH_WEB
  static void formHandler_emptyfields(String& message) /*override*/;
  static void formHandler_TH(String& message, bool includeConfig) /*override*/;
  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  void formHandler_TD(String& message, bool includeConfig) override;
  bool formHandler_args(IotsaWebServer *server, const String& name, bool includeConfig) override;
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
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
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
