#include "iotsaUser.h"
#include "iotsaConfigFile.h"


IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_username, const char *_password)
:	IotsaAuthMod(_app),
  username(_username),
	password(_password)
#ifdef IOTSA_WITH_API
  ,
	api(this, _app, this)
#endif
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void
IotsaUserMod::handler() {
  bool anyChanged = false;
  bool passwordChanged = false;
  bool oldPasswordCorrect = false;
  bool newPasswordsMatch = false;
  String newUsername;
  
  if( server->hasArg("username")) {
    newUsername = server->arg("username");
    if (newUsername == username) {
      // No change, really.
      newUsername = "";
      anyChanged = false;
    } else {
      if (needsAuthentication("users")) return;
      anyChanged = true;
    }
  }
  if( server->hasArg("password")) {
    // password authentication is checked later.
    String pw1 = server->arg("password");
    String pw2 = server->arg("again");
    String old = server->arg("old");
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
  server->send(200, "text/html", message);
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

#ifdef IOTSA_WITH_API
bool IotsaUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/users") == 0) {
    JsonArray users = reply["users"].as<JsonArray>();
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
#endif // IOTSA_WITH_API

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/users", std::bind(&IotsaUserMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/users", true, false, true);
  api.setup("/api/users/0", true, true, false);
  name = "users";
#endif
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
  if (server->authenticate(username.c_str(), password.c_str())) {
    return true;
  }
  server->requestAuthentication();
#endif
  return false;
}
