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
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
};

// Usernames, passwords and rights. No users means everything is allowed, as on a
// factory-fresh device. A non-empty defaultPassword gives a device without stored
// users the user defaultUsername, with all rights.
class IotsaUserMod;
// The users, on the web page and in the API (cwi-dis/iotsa#309), keyed by
// username. A new user is checked by the module.
class IotsaUserList : public IotsaVector<IotsaUser> {
public:
  IotsaUserList(IotsaUserMod& mod, std::vector<IotsaUser>& users)
  : IotsaVector<IotsaUser>("users", users, Add | Remove, Sections, IotsaContext::SavedInConfigMode),
    _mod(mod)
  {}
  String key(int i) override { return _v[i].username; }
  const char *keyField() override { return "username"; }
protected:
  bool accept(IotsaUser& user, IotsaContext& ctx) override;
private:
  IotsaUserMod& _mod;
};

class IotsaUserMod : public IotsaAuthMod {
public:
  IotsaUserMod(IotsaApplication &_app, const char *defaultUsername="admin", const char *defaultPassword="");
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  IotsaAccess allows(IotsaContext& ctx, const char *right) override;
  void putHandler(IotsaApiContext& ctx) override;
  void postHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
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
  friend class IotsaUserList;
  IotsaUserList userList{*this, users};
  // Why newUser can't be added, or nullptr.
  const char *_refuseUser(const IotsaUser& newUser);
};
#endif
