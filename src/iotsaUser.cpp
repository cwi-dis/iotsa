#include "iotsaUser.h"
#include "iotsaConfigFile.h"

bool IotsaUser::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  cf.get(f_name + ".username", username, "");
  cf.get(f_name + ".password", password, "");
  cf.get(f_name + ".rights", rights, "");
  return username != "";
}

void IotsaUser::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".username", username);
  cf.put(f_name + ".password", password);
  cf.put(f_name + ".rights", rights);
}

// The username isn't changed here: it addresses the user.
// Inside the module's Change, so the right and the mode were checked before
// the old password is compared.
void IotsaUser::putHandler(IotsaApiContext& ctx) {
  // A user with a password is only changed by whoever knows it.
  if (password != "") {
    ctx.used("old_password");
    if (ctx.request()["old_password"].as<String>() != password) {
      ctx.error(403, "wrong old_password");
      return;
    }
  }
  ctx.argPassword("password", password, IotsaApiContext::SavedInConfigMode);
  ctx.arg("rights", rights, IotsaApiContext::SavedInConfigMode);
}

void IotsaUser::getHandler(IotsaApiContext& ctx) {
  ctx.show("username", username);
  ctx.show("rights", rights);
  ctx.showsNeedRight(IOTSA_RIGHT_NEVER);
  ctx.show("password", password);
}

#ifdef IOTSA_WITH_WEB
// Who controls the device: configuration mode, as over REST (cwi-dis/iotsa#183).
// Only a new user (in the add form) has no username yet.
void IotsaUser::formHandler_args(IotsaWebContext& ctx) {
  if (username == "") ctx.arg("username", username, IotsaWebContext::SavedInConfigMode);
  ctx.argPassword("password", password, IotsaWebContext::SavedInConfigMode);
  ctx.arg("rights", rights, IotsaWebContext::SavedInConfigMode);
}

void IotsaUser::formHandler_fields(IotsaWebContext& ctx) {
  // An existing user's username is the heading of its section.
  if (username == "") ctx.showField("Username", "username", username, IotsaWebContext::SavedInConfigMode);
  ctx.showPassword("Password", "password", IotsaWebContext::SavedInConfigMode, password == "" ? "" : "empty: unchanged");
  ctx.showField("Rights", "rights", rights, IotsaWebContext::SavedInConfigMode, "* for all, or /right/right/");
}
#endif

IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_defaultUsername, const char *_defaultPassword)
:	IotsaAuthMod(_app),
  defaultUsername(_defaultUsername),
  defaultPassword(_defaultPassword)
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
bool IotsaUserList::accept(IotsaUser& user, IotsaWebContext& ctx) {
  const char *err = _mod._refuseUser(user);
  if (err) ctx.error(err);
  return err == nullptr;
}
#endif

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::lateSetup() {
  api.setup("users", true, true, true);
  name = "users";
}

void IotsaUserMod::loop() {
}

IotsaAccess IotsaUserMod::allows(IotsaContext& ctx, const char *right) {
  // No users means everything is allowed.
  if (users.empty()) return IotsaAccess::Granted;
  String rightField = String("/") + right + "/";
  for (auto& u: users) {
    if (ctx.hasBasicAuth(u.username.c_str(), u.password.c_str())) {
      if (u.rights == "*" || u.rights.indexOf(rightField) >= 0) return IotsaAccess::Granted;
      // A known user without the right: logging in again won't help.
      return IotsaAccess::Forbidden;
    }
  }
  return IotsaAccess::NeedsCredentials;
}

const char *IotsaUserMod::_refuseUser(const IotsaUser& newUser) {
  if (newUser.username == "") return "A new user needs a username.";
  if (newUser.password == "") return "A new user needs a password.";
  for (auto& u : users) {
    if (u.username == newUser.username) return "That user already exists.";
  }
  return nullptr;
}

int IotsaUserMod::_find(const String& username) {
  for (int i = 0; i < (int)users.size(); i++) {
    if (users[i].username == username) return i;
  }
  return -1;
}

void IotsaUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  users.clear();
  for(int i=0; ; i++) {
    String f_name = String(i);
    IotsaUser newUser;
    if (!newUser.configLoad(cf, f_name)) break;
    users.push_back(newUser);
  }
  // Not saved: removing the last user brings it back at the next boot.
  if (users.empty() && defaultPassword != "") {
    IotsaUser u;
    u.username = defaultUsername;
    u.password = defaultPassword;
    u.rights = "*";
    users.push_back(u);
  }
}

void IotsaUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");

  int i = 0;
  for(auto& it: users) {
    String f_name = String(i++);
    it.configSave(cf, f_name);
  }
}

// Change one user, addressed by username, or remove it with "remove": true.
// Change or remove the user named by username. A Change, as on the web page:
// the right and configuration mode are checked before anything else.
void IotsaUserMod::putHandler(IotsaApiContext& ctx) {
  ctx.argsNeedRight("users");
  IotsaApiContext::Change change(ctx, "username", IotsaApiContext::SavedInConfigMode);
  if (!change) {
    if (!ctx.failed()) ctx.error(400, "username missing");
    return;
  }
  int idx = _find(ctx.request()["username"].as<String>());
  if (idx < 0) {
    ctx.error(404, "no such user");
    return;
  }
  bool remove = false;
  ctx.arg("remove", remove, IotsaApiContext::SavedInConfigMode);
  if (!remove) users[idx].putHandler(ctx);
  if (!change) return;
  if (remove) users.erase(users.begin() + idx);
  change.done();
}

// Add a user.
// Add a user.
void IotsaUserMod::postHandler(IotsaApiContext& ctx) {
  ctx.argsNeedRight("users");
  IotsaApiContext::Change change(ctx, "username", IotsaApiContext::SavedInConfigMode);
  if (!change) {
    if (!ctx.failed()) ctx.error(400, "username missing");
    return;
  }
  IotsaUser newUser;
  ctx.arg("username", newUser.username, IotsaApiContext::SavedInConfigMode);
  newUser.putHandler(ctx);
  if (!change) return;
  const char *err = _refuseUser(newUser);
  if (err) {
    ctx.error(400, err);
    return;
  }
  users.push_back(newUser);
  change.done();
}

void IotsaUserMod::getHandler(IotsaApiContext& ctx) {
  JsonArray usersList = ctx.reply()["users"].to<JsonArray>();
  for (auto& u: users) {
    IotsaApiContext::Scope s(ctx, u.username, JsonObject(), usersList.add<JsonObject>());
    u.getHandler(ctx);
  }
}

#ifdef IOTSA_WITH_WEB
void IotsaUserMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.argsNeedRight("users");
  IotsaWebContext::Scope s(ctx, "users");
  webUsers.formHandler_args(ctx);
}

void IotsaUserMod::formHandler_fields(IotsaWebContext& ctx) {
  ctx.argsNeedRight("users");
  if (ctx.includesEveryday()) {
    ctx.showStatus("Users", "", users.empty() ? String("none, everything is allowed") : String(users.size()));
    return;
  }
  IotsaWebContext::Scope s(ctx, "users");
  webUsers.formHandler_fields(ctx);
}

String IotsaUserMod::info() {
  String message = "<p>Usernames, passwords and rights ";
  message += users.empty() ? "supported, but no users yet." : "enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB
