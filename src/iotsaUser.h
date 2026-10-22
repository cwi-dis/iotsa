#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include <vector>
#include "iotsa.h"
#include "iotsaApi.h"

// One user. The username is set when the user is added and can't change: it is
// how the user is addressed, on the web page and in REST (cwi-dis/iotsa#217).
class IotsaUser : public IotsaApiModObject {
public:
  IotsaUser() {}
  String username;
  String password;
  String rights;
public:
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
  bool putHandler(const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool getHandler(JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
};

// Usernames, passwords and rights. No users means everything is allowed, as on a
// factory-fresh device. A non-empty defaultPassword gives a device without stored
// users the user defaultUsername, with all rights.
class IotsaUserMod : public IotsaAuthMod {
public:
  IotsaUserMod(IotsaApplication &_app, const char *defaultUsername="admin", const char *defaultPassword="");
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  bool getHandler(const char *path, JsonObject& reply) override;
protected:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Edit users"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  std::vector<IotsaUser> users;
  String defaultUsername;
  String defaultPassword;
#ifdef IOTSA_WITH_WEB
  IotsaWebVector<IotsaUser> webUsers;
#endif
  // Why newUser can't be added, or nullptr.
  const char *_refuseUser(const IotsaUser& newUser);
  int _find(const String& username);
};
#endif
