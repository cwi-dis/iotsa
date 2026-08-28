#include "iotsaUser.h"
#include "iotsaConfigFile.h"


IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_username, const char *_password)
:	IotsaAuthMod(_app),
  username(_username),
	password(_password),
	api(this, _app, this)
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void
IotsaUserMod::webHandler() {
  bool anyChanged = false;
  bool passwordChanged = false;
  bool oldPasswordCorrect = false;
  bool newPasswordsMatch = false;
  String newUsername;
  
  if( api.webService->server->hasArg("username")) {
    newUsername = api.webService->server->arg("username");
    if (newUsername == username) {
      // No change, really.
      newUsername = "";
      anyChanged = false;
    } else {
      if (needsAuthentication("users")) return;
      anyChanged = true;
    }
  }
  if( api.webService->server->hasArg("password")) {
    // password authentication is checked later.
    String pw1 = api.webService->server->arg("password");
    String pw2 = api.webService->server->arg("again");
    String old = api.webService->server->arg("old");
    oldPasswordCorrect = (old == password);
    newPasswordsMatch = (pw1 == pw2);
    if (oldPasswordCorrect && newPasswordsMatch) {
      password = pw1;
      passwordChanged = true;
      anyChanged = true;
    }
  }
  if (anyChanged && newUsername != "") {
  	username = newUsername;
  }
  if (anyChanged) configSave();
  
  String message = "<html><head><title>Edit users and passwords</title></head><body><h1>Edit users and passwords</h1>";
  if (anyChanged && !passwordChanged) {
  	message += "<p><em>Username changed.</em></p>";
  } else if (passwordChanged && anyChanged) {
  	message += "<p><em>Password has been changed.</em></p>";
  } else if (passwordChanged && !oldPasswordCorrect) {
  	message += "<p><em>Old password incorrect.</em></p>";
  } else if (passwordChanged && !newPasswordsMatch) {
  	message += "<p><em>Passwords do not match, not changed.</em></p>";
  }
  	
  message += "<form method='get'>Username: <input name='username' value='";
  message += htmlEncode(username);
  message += "'>";
  if (password != "") {
    message += "<br>Old Password: <input type='password' name='old' value=''";
    message += "";
    message += "'>";
  }
  message += "<br>New Password: <input type='password' name='password' value='";
  message += "";
  message += "'><br>Repeat New Password: <input type='password' name='again' value='";
  message += "";
  message += "'><br><input type='submit'></form>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaUserMod::info() {
  String message = "<p>Username/password protection ";
  if (username == "" || password == "") {
    message += "supported, but not currently enabled.";
  } else {
    message += "enabled.";
  }
  message += " See <a href=\"/users\">/users</a> to change.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") == 0) {
    JsonArray users = reply["users"].to<JsonArray>();
    JsonObject user = users.add<JsonObject>();
    user["username"] = username;
    bool hasPassword = password.length() > 0;
    user["hasPassword"] = hasPassword;
    return true;
  }
  return false;
}

bool IotsaUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  // PUT to /api/users is equivalent to POST /api/users/0 (because of iotsaControl issues)
  if (strcmp(path, "/api/users") != 0) return false;
  return putHandler("/api/users/0", request, reply);
}

bool IotsaUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/users/0") != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  // Check old password, if a password has been set.
  if (password) {
    String old = reqObj["old_password"].as<String>();
    if (old != password) return false;
  }
  if (getFromRequest<const char *>(reqObj, "username", username)) {
    anyChanged = true;
  }
  if (getFromRequest<const char *>(reqObj, "password", password)) {
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::lateSetup() {
  api.setup("users", true, false, true);
  // webPage=false: webHandler() doesn't distinguish by path, so a page here would
  // just duplicate "users" -- see cwi-dis/iotsa#217.
  api.setup("users/0", true, true, false, false);
  name = "users";
}

void IotsaUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  cf.get("0.user", username, username);
  cf.get("0.password", password, password);
  IotsaSerial.print("Loaded users.cfg. Username=");
  IotsaSerial.print(username);
  IotsaSerial.print(", password length=");
  IotsaSerial.println(password.length());
}

void IotsaUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");
  cf.put("0.user", username);
  cf.put("0.password", password);
  IotsaSerial.print("Saved users.cfg. Username=");
  IotsaSerial.print(username);
  IotsaSerial.print(", password length=");
  IotsaSerial.println(password.length());
}

void IotsaUserMod::loop() {
}

bool IotsaUserMod::allows(const char *right) {
  // We ignore "right", username/password grants all rights.
  if (password == "" || username == "")
    return true;
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  // Reaches app.server directly rather than through api.webService: allows() is the
  // transport-agnostic IotsaAuthenticationProvider interface, called for every
  // module/transport, not just this one's own web page -- a known wart pending
  // cwi-dis/iotsa#107's context-object redesign, see cwi-dis/iotsa#211.
  if (app.server->authenticate(username.c_str(), password.c_str())) {
    return true;
  }
  app.server->requestAuthentication();
#endif
  return false;
}
