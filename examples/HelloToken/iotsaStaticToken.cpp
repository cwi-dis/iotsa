#include "iotsaUser.h"
#include "iotsaConfigFile.h"
#include "iotsaStaticToken.h"

#undef PASSWORD_DEBUGGING	// Enables admin1/admin1 to always log in

class StaticToken {
public:
  String token;
  String rights;
};

IotsaStaticTokenMod::IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthMod &_chain)
:	chain(_chain),
	IotsaAuthMod(_app)
{
	configLoad();
}
	
void
IotsaStaticTokenMod::handler() {
#if 0
  bool anyChanged = false;
  String pwold, pw1, pw2;
  bool passwordChanged = false;
  
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "username") {
    	String un = server.arg(i);
    	if (un != username) {
			if (needsAuthentication()) return;
			username = un;
	    	anyChanged = true;
		}
    }
    if( server.argName(i) == "old") {
    	// password authentication is checked later.
    	pwold = server.arg(i);
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
  	if (pwold != password || pw1 != pw2) {
  		// Old password incorrect or passwords don't match
  		anyChanged = false;
  	} else {
		password = pw1;
		anyChanged = true;
	}
  }
  if (anyChanged) configSave();
  String message = "<html><head><title>Edit users and passwords</title></head><body><h1>Edit users and passwords</h1>";
  if (passwordChanged && !anyChanged) {
  	message += "<p><em>Passwords do not match, not changed.</em></p>";
  } else if (passwordChanged) {
  	message += "<p><em>Password has been changed.</em></p>";
  }
  	
  message += "<form method='get'>Username: <input name='username' value='";
  message += htmlEncode(username);
  message += "'>";
  if (password != "") {
  	message += "<br>Old Password: <input type='password' name='old' value=''";
  	message += "empty1";
  	message += "'>";
  } else if (configurationMode == TMPC_CONFIG) {
  	message += "<br>Password not set, default is '";
  	message += defaultPassword();
  	message += "'.";
  } else {
  	message += "<br>Password not set, reboot in configuration mode to see default password.";
  }
  message += "<br>New Password: <input type='password' name='password' value='";
  message += "empty2";
  message += "'><br>Repeat New Password: <input type='password' name='again' value='";
  message += "empty3";
  message += "'><br><input type='submit'></form>";
  server.send(200, "text/html", message);
#endif
}

void IotsaStaticTokenMod::setup() {
  configLoad();
}

void IotsaStaticTokenMod::serverSetup() {
  server.on("/tokens", std::bind(&IotsaStaticTokenMod::handler, this));
}

void IotsaStaticTokenMod::configLoad() {
  IotsaConfigFileLoad cf("/config/statictokens.cfg");
  cf.get("ntoken", ntoken, 0);
  if (tokens) free(tokens);
  tokens = calloc(ntoken, sizeof StaticToken);
   
  for (int i=0; i<ntoken; i++) {
    String tokenValue;
    String tokenRights;
    cf.get("token" + String(i), tokens[i].token, "");
  	cf.get("rights" + String(i), tokens[i].rights, "");
  	// xxxjack store into a token object
}

void IotsaStaticTokenMod::configSave() {
  IotsaConfigFileSave cf("/config/statictokens.cfg");
  cp.put("ntoken", ntoken);
  for (int i=0; i<ntoken; i++) {
	cf.put("token" + String(i), tokens[i].token);
  	cf.put("rights" + String(i), tokens[i].rights);
  }
}

void IotsaStaticTokenMod::loop() {
}

String IotsaStaticTokenMod::info() {
  String message = "<p>Static tokens enabled.";
  message += " See <a href=\"/tokens\">/tokens</a> to change.";
  message += "</p>";
  return message;
}

bool IotsaStaticTokenMod::needsAuthentication(const char *right) {
  // Check whether a token is present in the headers/args
  // Check whether it provides the correct right, return false if so
  return chain->needsAuthentication(right);
}
