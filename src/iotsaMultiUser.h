#ifndef _IOTSAUSERS_H_
#define _IOTSAUSERS_H_
#include <vector>
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaUser : public IotsaApiModObject {
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
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  bool getHandler(JsonObject& reply) override;
  bool putHandler(const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
};

class IotsaMultiUserMod : public IotsaAuthMod {
public:
  IotsaMultiUserMod(IotsaApplication &_app);
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
  bool getHandler(const char *path, JsonObject& reply) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
protected:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Edit users"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  std::vector<IotsaUser> users;
#ifdef IOTSA_WITH_WEB
  // No Remove: REST addresses users by index (/api/users/<n>), registered at
  // boot, cwi-dis/iotsa#217.
  IotsaWebVector<IotsaUser> webUsers;
  bool _acceptUser(IotsaUser& newUser, IotsaWebContext& ctx);
#endif
  int _addUser(IotsaUser& newUser);
  // _delUser(int) not implemented, because of /api/users/<num> which would change
};

#endif
