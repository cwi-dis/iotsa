#include "iotsaUser.h"
#include "iotsaConfigFile.h"


IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_username, const char *_password)
:	IotsaAuthMod(_app),
  username(_username),
	password(_password)
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void IotsaUserMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.needsRight("users");
  ctx.arg("username", username);
  // Changing the password needs no login: knowing the old one is the proof.
  // The form always sends the three fields; typing the old password with empty
  // new ones clears the password.
  ctx.needsRight(IOTSA_RIGHT_NONE);
  String newPassword, again, old;
  bool attempted = ctx.text("password", newPassword);
  attempted |= ctx.text("again", again);
  attempted |= ctx.text("old", old);
  if (!attempted) return;
  if (old != password) {
    ctx.error("Old password incorrect.");
  } else if (newPassword != again) {
    ctx.error("Passwords do not match, not changed.");
  } else {
    password = newPassword;
    ctx.changed(IotsaWebContext::Config);
    ctx.feedback(password == "" ? "Password cleared." : "Password changed.");
  }
}

void IotsaUserMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section == IotsaWebContext::Everyday) {
    ctx.status("Username/password protection", "", (username == "" || password == "") ? "not enabled" : "enabled");
    return;
  }
  IotsaWebContext::Form form(ctx);
  ctx.field("Username", "username", username);
  if (password != "") ctx.password("Old password", "old");
  ctx.password("New password", "password", IotsaWebContext::Config, "old password and empty new ones: no password");
  ctx.password("Repeat new password", "again");
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

bool IotsaUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  // PUT to /api/users is equivalent to POST /api/users/0 (because of iotsaControl issues)
  if (strcmp(path, "/api/users") != 0) return apiError(404, "not found");
  return putHandler("/api/users/0", request, reply, changed);
}

bool IotsaUserMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (strcmp(path, "/api/users/0") != 0) return apiError(404, "not found");
  if (!iotsaController.inConfigurationMode()) return apiError(409, "not in configuration mode");
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  // Check old password, if a password has been set.
  if (password) {
    String old = reqObj["old_password"].as<String>();
    if (old != password) return apiError(403, "wrong old_password");
  }
  if (getFromRequest<const char *>(reqObj, "username", username)) {
    anyChanged = true;
  }
  if (getFromRequest<const char *>(reqObj, "password", password)) {
    anyChanged = true;
  }
  if (anyChanged) { configSave(); if (changed) *changed = true; }
  return true;
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
#ifdef IOTSA_HAS_WEBSERVER
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
