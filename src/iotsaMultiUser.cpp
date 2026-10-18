#include "iotsaMultiUser.h"
#include "iotsaConfigFile.h"

bool IotsaUser::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  cf.get(f_name + ".username", username, "");
  cf.get(f_name + ".password", password, "");
  cf.get(f_name + ".rights", rights, "");
  // xxxjack cf.get(f_name + ".apiEndpoint", apiEndpoint, "");
  return username != "";
}

void IotsaUser::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".username", username);
  cf.put(f_name + ".password", password);
  cf.put(f_name + ".rights", rights);
}

#ifdef IOTSA_WITH_WEB
// Who controls the device: configuration mode, as over REST (cwi-dis/iotsa#183).
void IotsaUser::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("username", username, IotsaWebContext::ConfigModeOnly);
  ctx.password("password", password, IotsaWebContext::ConfigModeOnly);
  ctx.arg("rights", rights, IotsaWebContext::ConfigModeOnly);
}

void IotsaUser::formHandler_fields(IotsaWebContext& ctx) {
  ctx.field("Username", "username", username, IotsaWebContext::ConfigModeOnly);
  ctx.password("Password", "password", IotsaWebContext::ConfigModeOnly, username == "" ? "empty: no password" : "empty: unchanged");
  ctx.field("Rights", "rights", rights, IotsaWebContext::ConfigModeOnly, "* for all, or /right/right/");
}
#endif

bool IotsaUser::getHandler(JsonObject& reply) {
  reply["username"] = username;
  bool hasPassword = password.length() > 0;
  reply["has_password"] = hasPassword;
  reply["rights"] = rights;
  return true;
}

bool IotsaUser::putHandler(const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  JsonObject reqObj = request.as<JsonObject>();
  if (password) {
    String old = reqObj["old_password"].as<String>();
    if (old != password) return apiError(403, "wrong old_password");
  }
  bool any = false;
  any |= getFromRequest<const char *>(reqObj, "username", username);
  any |= getFromRequest<const char *>(reqObj, "password", password);
  any |= getFromRequest<const char *>(reqObj, "rights", rights);
  if (any && changed) *changed = true;
  return true;
}

IotsaMultiUserMod::IotsaMultiUserMod(IotsaApplication &_app)
:	IotsaAuthMod(_app)
#ifdef IOTSA_WITH_WEB
  , webUsers(users, IotsaWebCollection::Add, IotsaWebCollection::Sections, IotsaWebContext::ConfigModeOnly,
      [](IotsaUser& u) { return u.username; },
      [this](IotsaUser& u, IotsaWebContext& ctx) { return _acceptUser(u, ctx); })
#endif
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void IotsaMultiUserMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.needsRight("users");
  IotsaWebContext::Scope s(ctx, "users");
  webUsers.formHandler_args(ctx);
}

void IotsaMultiUserMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section == IotsaWebContext::Everyday) {
    ctx.status("Users", "", String(users.size()));
    return;
  }
  if (!ctx.editable(IotsaWebContext::ConfigModeOnly)) {
    ctx << "<p>Users can only be changed in configuration mode, see <a href='/runmode'>/runmode</a>.</p>\n";
  }
  IotsaWebContext::Scope s(ctx, "users");
  webUsers.formHandler_fields(ctx);
}

bool IotsaMultiUserMod::_acceptUser(IotsaUser& newUser, IotsaWebContext& ctx) {
  if (newUser.username == "") {
    ctx.error("A new user needs a username.");
    return false;
  }
  for (auto& u : users) {
    if (u.username == newUser.username) {
      ctx.error("User " + newUser.username + " already exists.");
      return false;
    }
  }
  newUser.apiEndpoint = String("/api/users/") + String(users.size());
  return true;
}

String IotsaMultiUserMod::info() {
  String message = "<p>Multiple Usernames/passwords/rights enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaMultiUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") == 0) {
    reply["multi"] = true;
    JsonArray usersList = reply["users"].to<JsonArray>();
    for (auto u: users) {
      JsonObject user = usersList.add<JsonObject>();
      u.getHandler(user);
    }
    return true;
  }
  for (auto u: users) {
    if (strcmp(u.apiEndpoint.c_str(), path) == 0) {
      u.getHandler(reply);
      return true;
    }
  }
  return false;
}

bool IotsaMultiUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (strncmp(path, "/api/users/", 11) != 0) return apiError(404, "not found");
  if (!iotsaConfigSettingsWritable()) return apiError(409, "not in configuration mode");
  // xxxjack should also check access rights? Maybe in stead of configurationMode?
  String num(path);
  num.remove(0, 11);
  int idx = num.toInt();
  if (idx < 0 || idx >= (int)users.size()) return apiError(404, "no such user");

  IotsaUser& u = users[idx];
  bool userChanged = false;
  if (!u.putHandler(request, reply, &userChanged)) return false;
  if (userChanged) {
    configSave();
    if (changed) *changed = true;
  }
  return true;
}
bool IotsaMultiUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (strcmp(path, "/api/users") != 0) return apiError(404, "not found");
  if (!iotsaConfigSettingsWritable()) return apiError(409, "not in configuration mode");
  IotsaUser newUser;
  if (!newUser.putHandler(request, reply)) return false;
  if (newUser.username == "") return apiError(400, "username missing");
  _addUser(newUser);
  configSave();
  if (changed) *changed = true;
  return true;
}

int IotsaMultiUserMod::_addUser(IotsaUser& newUser) {
  int oldLength = users.size();
  String bareName = String("users/")+String(oldLength);
  newUser.apiEndpoint = String("/api/") + bareName;
  users.push_back(newUser);
  // webPage=false: webHandler() doesn't distinguish by path, so a page here would
  // just duplicate "users" -- see cwi-dis/iotsa#217.
  api.setup(bareName.c_str(), true, true, false, false);
  configSave();
  return oldLength;
}

void IotsaMultiUserMod::setup() {
  configLoad();
}

void IotsaMultiUserMod::lateSetup() {
  api.setup("users", true, false, true);
  name = "users";
  int idx = 0;
  for(auto& u: users) {
    String bareName = String("users/")+String(idx++);
    u.apiEndpoint = String("/api/") + bareName;
    // webPage=false: webHandler() doesn't distinguish by path, so a page here would
    // just duplicate "users" -- see cwi-dis/iotsa#217.
    api.setup(bareName.c_str(), true, true, false, false);
  }
}

void IotsaMultiUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  users.clear();
  for(int i=0; ; i++) {
    String f_name = String(i);
    IotsaUser newUser;
    if (!newUser.configLoad(cf, f_name)) break;
    users.push_back(newUser);
  }
}

void IotsaMultiUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");

  int i = 0;
  for(auto it: users) {
    String f_name = String(i++);
    it.configSave(cf, f_name);
  }
}

void IotsaMultiUserMod::loop() {
}

bool IotsaMultiUserMod::allows(const char *right) {
  // No users means everything is allowed.
  if (users.empty()) return true;
#ifdef IOTSA_HAS_WEBSERVER
  // Reaches app.server directly rather than through api.webService: allows() is the
  // transport-agnostic IotsaAuthenticationProvider interface, called for every
  // module/transport, not just this one's own web page -- a known wart pending
  // cwi-dis/iotsa#107's context-object redesign, see cwi-dis/iotsa#211.
  // Otherwise we loop over all users until we find one that matches.
  for(auto u: users) {
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

bool IotsaMultiUserMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}