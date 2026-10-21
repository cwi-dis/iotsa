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

bool IotsaUser::getHandler(JsonObject& reply) {
  reply["username"] = username;
  reply["hasPassword"] = password.length() > 0;
  reply["rights"] = rights;
  return true;
}

// The username isn't changed here: it addresses the user.
bool IotsaUser::putHandler(const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  JsonObject reqObj = request.as<JsonObject>();
  if (password != "") {
    String old = reqObj["old_password"].as<String>();
    if (old != password) return apiError(403, "wrong old_password");
  }
  bool any = false;
  any |= getFromRequest<const char *>(reqObj, "password", password);
  any |= getFromRequest<const char *>(reqObj, "rights", rights);
  if (any && changed) *changed = true;
  return true;
}

IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_defaultUsername, const char *_defaultPassword)
:	IotsaAuthMod(_app),
  defaultUsername(_defaultUsername),
  defaultPassword(_defaultPassword)
#ifdef IOTSA_WITH_WEB
  , webUsers(users, IotsaWebCollection::Add | IotsaWebCollection::Remove, IotsaWebCollection::Sections, IotsaWebContext::SavedInConfigMode,
      [](IotsaUser& u) { return u.username; },
      [this](IotsaUser& u, IotsaWebContext& ctx) {
        const char *err = _refuseUser(u);
        if (err) ctx.error(err);
        return err == nullptr;
      })
#endif
{
	configLoad();
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

#ifdef IOTSA_WITH_WEB
void IotsaUserMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.argsNeedRight("users");
  IotsaWebContext::Scope s(ctx, "users");
  webUsers.formHandler_args(ctx);
}

void IotsaUserMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesEveryday()) {
    ctx.showStatus("Users", "", users.empty() ? String("none, everything is allowed") : String(users.size()));
    return;
  }
  if (!ctx.editable(IotsaWebContext::SavedInConfigMode)) {
    ctx << "<p>Users can only be changed in configuration mode, see <a href='/runmode'>/runmode</a>.</p>\n";
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

bool IotsaUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") != 0) return false;
  JsonArray usersList = reply["users"].to<JsonArray>();
  for (auto& u: users) {
    JsonObject user = usersList.add<JsonObject>();
    u.getHandler(user);
  }
  return true;
}

// Change one user, addressed by username, or remove it with "remove": true.
bool IotsaUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (strcmp(path, "/api/users") != 0) return apiError(404, "not found");
  if (!iotsaConfigSettingsWritable()) return apiError(409, "not in configuration mode");
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  JsonObject reqObj = request.as<JsonObject>();
  int idx = _find(reqObj["username"].as<String>());
  if (idx < 0) return apiError(404, "no such user");
  bool userChanged = false;
  if (reqObj["remove"].as<bool>()) {
    users.erase(users.begin() + idx);
    userChanged = true;
  } else {
    if (!users[idx].putHandler(request, reply, &userChanged)) return false;
  }
  if (userChanged) {
    configSave();
    if (changed) *changed = true;
  }
  return true;
}

// Add a user.
bool IotsaUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (strcmp(path, "/api/users") != 0) return apiError(404, "not found");
  if (!iotsaConfigSettingsWritable()) return apiError(409, "not in configuration mode");
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  IotsaUser newUser;
  newUser.username = request["username"].as<String>();
  if (!newUser.putHandler(request, reply)) return false;
  const char *err = _refuseUser(newUser);
  if (err) return apiError(400, err);
  users.push_back(newUser);
  configSave();
  if (changed) *changed = true;
  return true;
}

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::lateSetup() {
  api.setup("users", true, true, true);
  name = "users";
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

void IotsaUserMod::loop() {
}

bool IotsaUserMod::allows(const char *right) {
  // No users means everything is allowed.
  if (users.empty()) return true;
#ifdef IOTSA_HAS_WEBSERVER
  // Reaches app.server directly rather than through api.webService: allows() is the
  // transport-agnostic IotsaAuthenticationProvider interface, called for every
  // module/transport, not just this one's own web page -- a known wart pending
  // cwi-dis/iotsa#107's context-object redesign, see cwi-dis/iotsa#211.
  // Otherwise we loop over all users until we find one that matches.
  for(auto& u: users) {
    if (app.server->authenticate(u.username.c_str(), u.password.c_str())) {
      // NULL or empty rights field means: only existence is required.
      if (right == NULL || *right == '\0') return true;
      String rightField("/");
      rightField += right;
      rightField += "/";
      if (u.rights == "*" || u.rights.indexOf(rightField) >= 0) {
        return true;
      }
      break;
    }
  }
  app.server->requestAuthentication();
  IotsaSerial.print("Return 401 Unauthorized for right=");
  IotsaSerial.println(right);

#endif // WITH_HTTP_OR_HTTPS
  return false;
}

bool IotsaUserMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}
