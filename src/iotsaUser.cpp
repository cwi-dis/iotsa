#include "iotsaUser.h"
#include "iotsaConfigFile.h"

#undef PASSWORD_DEBUGGING	// Enables admin1/admin1 to always log in

String &defaultPassword() {
  static String dftPwd;
  if (dftPwd == "") {
#ifdef ESP32
	  randomSeed(ESP.getEfuseMac());
#else
	  randomSeed(ESP.getChipId());
#endif
	  dftPwd = String("password") + String(random(1000));
  }
  return dftPwd;
}

IotsaUserMod::IotsaUserMod(IotsaApplication &_app, const char *_username, const char *_password)
:	username(_username),
	password(_password),
	IotsaAuthMod(_app)
{
	configLoad();
}
	
void
IotsaUserMod::handler() {
  bool anyChanged = false;
  String pwold, pw1, pw2;
  bool passwordChanged = false;
  bool oldPasswordCorrect = false;
  bool newPasswordsMatch = false;
  String newUsername;
  
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "username") {
    	newUsername = server.arg(i);
    	if (newUsername == username) {
    		newUsername = "";
    		anyChanged = false;
    	} else {
			if (needsAuthentication("users")) return;
	    	anyChanged = true;
		}
    }
    if( server.argName(i) == "old") {
    	// password authentication is checked later.
    	pwold = server.arg(i);
    	oldPasswordCorrect = pwold == password;
    }
    if( server.argName(i) == "password") {
    	// password authentication is checked later.
    	pw1 = server.arg(i);
    	passwordChanged = true;
    }
    if( server.argName(i) == "again") {
    	pw2 = server.arg(i);
    	passwordChanged = true;
    }
  }
  if (passwordChanged) {
	  newPasswordsMatch = pw1 == pw2;
	  if (newPasswordsMatch && oldPasswordCorrect) {
	  	password = pw1;
	  	anyChanged = true;
	  } else {
	  	anyChanged = false;
	  }
  }
  if (anyChanged && newUsername != "") {
  	username = newUsername;
  }
  if (anyChanged) configSave();
  
  String message = "<html><head><title>Edit users and passwords</title></head><body><h1>Edit users and passwords</h1>";
  if (anyChanged && newUsername != "") {
  	message += "<p><em>Username changed.</e,></p>";
  }
  if (passwordChanged && anyChanged) {
  	message += "<p><em>Password has been changed.</em></p>";
  } else if (passwordChanged && !oldPasswordCorrect) {
  	message += "<p><em>Old password incorrect.</em></p>";
  } else if (passwordChanged && !newPasswordsMatch) {
  	message += "<p><em>Passwords do not match, not changed.</em></p>";
  }
  	
  message += "<form method='get'>Username: <input name='username' value='";
  message += htmlEncode(username);
  message += "'>";
  message += "<br>Old Password: <input type='password' name='old' value=''";
  message += "empty1";
  message += "'>";
  if (password == "") {
	if (configurationMode == TMPC_CONFIG) {
	  message += "<br><i>(Password not set, default is '";
	  message += defaultPassword();
	  message += "')</i>";
	} else {
	  message += "<br><i>(Password not set, reboot in configuration mode to see default password)</i>)";
	}
  }
  message += "<br>New Password: <input type='password' name='password' value='";
  message += "empty2";
  message += "'><br>Repeat New Password: <input type='password' name='again' value='";
  message += "empty3";
  message += "'><br><input type='submit'></form>";
  server.send(200, "text/html", message);
}

#ifdef notyet
bool IotsaUserMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/user") == 0) {
    JsonArray& users = reply.createNestedArray("users");
    JsonObject& user = users.createNestedObject();
    user["username"] = username;
    return true;
  }
  return false;
}

bool IotsaUserMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/user/0") != 0) return false;
  if (configurationMode != TMPC_CONFIG) return false;
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  // Check old password, if a password has been set.
  if (password) {
    String old = reqObj.get<String>("old");
    if (old != password) return false;
  }
  if (reqObj.containsKey("username")) {
    username = reqObj.get<String>("username");
    anyChanged = true;
  }
  if (reqObj.containsKey("password")) {
    password = reqObj.get<String>("password");
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // notyet

void IotsaUserMod::setup() {
  configLoad();
}

void IotsaUserMod::serverSetup() {
  server.on("/users", std::bind(&IotsaUserMod::handler, this));
#ifdef notyet
  server.on("/api/users", true);
  server.on("/api/users/0", true, false, true);
#endif // notyet
}

void IotsaUserMod::configLoad() {
  IotsaConfigFileLoad cf("/config/users.cfg");
  cf.get("user0", username, username);
  cf.get("password0", password, password);
  IotsaSerial.print("Loaded users.cfg. Username=");
  IotsaSerial.print(username);
  IotsaSerial.print(", password length=");
  IotsaSerial.println(password.length());
}

void IotsaUserMod::configSave() {
  IotsaConfigFileSave cf("/config/users.cfg");
  cf.put("user0", username);
  cf.put("password0", password);
  IotsaSerial.print("Saved users.cfg. Username=");
  IotsaSerial.print(username);
  IotsaSerial.print(", password length=");
  IotsaSerial.println(password.length());
}

void IotsaUserMod::loop() {
}

String IotsaUserMod::info() {
  String message = "<p>Usernames/passwords enabled.";
  message += " See <a href=\"/users\">/users</a> to change.";
  if (configurationMode && password == "") {
  	message += "<br>Username and password are the defaults: '";
  	message += htmlEncode(username);
  	message += "' and '";
  	String &dfp = defaultPassword();
  	message += dfp;
  	message += "'.";
  }
  message += "</p>";
  return message;
}

bool IotsaUserMod::needsAuthentication(const char *right) {
  // We ignore "right", username/password grants all rights.
  String &curPassword = password;
  if (curPassword == "")
  	curPassword = defaultPassword();
  if (!server.authenticate(username.c_str(), curPassword.c_str())
#ifdef PASSWORD_DEBUGGING
	  && !server.authenticate("admin1", "admin1")
#endif
  	) {
  	server.sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  	server.send(401, "text/plain", "401 Unauthorized\n");
  	IotsaSerial.print("Return 401 Unauthorized for right=");
  	IotsaSerial.println(right);
  	return true;
  }
  return false;
}
