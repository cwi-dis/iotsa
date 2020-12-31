#include "iotsaMultiUser.h"
#include "iotsaConfigFile.h"

bool IotsaUser::configLoad(IotsaConfigFileLoad& cf, String& f_name) {
#if 0
  String username;
  String password;
  String rights;
  String apiEndpoint;
#endif
  cf.get(f_name + ".username", username, "");
  cf.get(f_name + ".password", password, "");
  cf.get(f_name + ".rights", rights, "");
  // xxxjack cf.get(f_name + ".apiEndpoint", apiEndpoint, "");
  return username != "";
}

void IotsaUser::configSave(IotsaConfigFileSave& cf, String& f_name) {
  cf.put(f_name + ".username", username);
  cf.put(f_name + ".password", password);
  cf.put(f_name + ".rights", rights);
}

#ifdef IOTSA_WITH_WEB
void IotsaUser::formHandler(String& message, String& text, String& f_name) {

}
void IotsaUser::formHandlerTD(String& message) {

}

bool IotsaUser::formArgHandler(IotsaWebServer *server, String name) {
  username = server->arg("username");
  password = server->arg("password");
  rights = server->arg("rights");
  return true;
}

#endif

#ifdef IOTSA_WITH_API
void IotsaUser::getHandler(JsonObject& reply) {
  reply["username"] = username;
  bool hasPassword = password.length() > 0;
  reply["has_password"] = hasPassword;
  reply["rights"] = rights;
}

bool IotsaUser::putHandler(const JsonVariant& request) {
  bool anyChanged;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("username")) {
    username = reqObj["username"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("password")) {
    password = reqObj["password"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("rights")) {
    rights = reqObj["rights"].as<String>();
    anyChanged = true;
  }
  return anyChanged;
}
#endif

IotsaMultiUserMod::IotsaMultiUserMod(IotsaApplication &_app)
:	IotsaAuthMod(_app),
  users(NULL)
#ifdef IOTSA_WITH_API
  ,
  api(this, _app, this)
#endif
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void
IotsaMultiUserMod::handler() {
  String command = server->arg("command");
  if (command == "") {
  
    String message = "<html><head><title>Edit users</title></head><body><h1>Edit users</h1>";
    message += "<table><tr><th>User</th><th>rights</th><th>New password<br>New rights</th></tr>";
    for(auto u: users) {
      message += "<tr>";
      u.formHandlerTD(message);
  #if 0
      message += "<td>";
      message += htmlEncode(u->username);
      message += "</td><td>";
      message += htmlEncode(u->rights);
      message += "</td><td><form method='get'>";
      message += "<form method='get'>";
      message += "<input type='hidden' name='command' value='add'>";
      message += "<input type='hidden' name='username' value='";
      message += htmlEncode(u->username);
      message += "'>";
      message += "<input name='password' type='password'><br>";
      message += "<input name='rights'><br>";
      message += "<input type='submit' value='Change'>";
      message += "</form></td>";
#endif
      message += "</tr>";
    }
    message += "</table><br><hr>";

    message += "<form method='get'>";
    message += "<input type='hidden' name='command' value='add'>";
    message += "Username: <input name='username'><br>";
    message += "Password: <input name='password' type='password'><br>";
    message += "Rights: <input name='rights'><br>";
    message += "<input type='submit' value='Add'>";
    message += "</form><hr>";

    server->send(200, "text/html", message);
    return;
  }

  if (needsAuthentication("users")) return;

  if (command == "add") {
    IotsaUser newUser;
    if (newUser.formArgHandler(server)) {
      _addUser(newUser);
    }
    server->send(200, "text/plain", "OK\r\n");
    return; 
  }
  if (command == "change") {
    String username = server->arg("username");
    for (auto u: users) {
      if (u.username == username) {
        if (i.formArgHandler(server)) {
          configSave();
          server->send(200, "text/plain", "OK\r\n");
          return; 
        }
      }
    }
    server->send(404, "text/plain", "No such user\r\n");
    return;
  }
  server->send(400, "text/plain", "Unknown command");
}

String IotsaMultiUserMod::info() {
  String message = "<p>Multiple Usernames/passwords/rights enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaMultiUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") == 0) {
    reply["multi"] = true;
    JsonArray usersList = reply.createNestedArray("users");
    for (auto u: users) {
      JsonObject user = usersList.createNestedObject();
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

bool IotsaMultiUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strncmp(path, "/api/users/", 11) != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  String num(path);
  num.remove(0, 11);
  int idx = num.toInt();

  bool anyChanged = false;
  IotsaUser& u = users[idx];
  JsonObject reqObj = request.as<JsonObject>();
  anyChanged = u.putHandler(reqObj);
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}
bool IotsaMultiUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/users") != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  bool anyChanged = false;
  IotsaUser newUser;
  JsonObject reqObj = request.as<JsonObject>();
  anyChanged = newUser.putHandler(reqObj);

  if (anyChanged) {
    _addUser(newUser);
    configSave();
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API

int IotsaMultiUserMod::_addUser(IotsaUser& newUser) {
  int oldLength = users.size();
  users.push_back(newUser);
  newUser.apiEndpoint = String("/api/users/")+String(oldLength);
  api.setup(newUser.apiEndpoint.c_str(), true, true, false);
  configSave();
  return oldLength;
}

void IotsaMultiUserMod::setup() {
  configLoad();
}

void IotsaMultiUserMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/users", std::bind(&IotsaMultiUserMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/users", true, false, true);
  name = "users";
  int idx = 0;
  for(auto u: users) {
    u.apiEndpoint = String("/api/users/")+String(idx++);
    api.setup(u.apiEndpoint.c_str(), true, true, false);
  }
#endif // IOTSA_WITH_API
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
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  // Otherwise we loop over all users until we find one that matches.
  for(auto u: users) {
    if (server->authenticate(u.username.c_str(), u.password.c_str())) {
      String rightField("/");
      rightField += right;
      rightField += "/";
      if (u.rights == "*" || u.rights.indexOf(rightField) >= 0) {
        return true;
      }
      break;
    }
  }
  server->requestAuthentication();
  IotsaSerial.print("Return 401 Unauthorized for right=");
  IotsaSerial.println(right);

#endif // WITH_HTTP_OR_HTTPS
  return false;
}

bool IotsaMultiUserMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}