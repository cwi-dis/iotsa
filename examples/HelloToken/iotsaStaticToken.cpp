#include "iotsaUser.h"
#include "iotsaConfigFile.h"
#include "iotsaStaticToken.h"

#undef PASSWORD_DEBUGGING	// Enables admin1/admin1 to always log in


bool IotsaStaticTokenObject::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  cf.get(f_name + ".token", token, "");
  cf.get(f_name + ".rights", rights, "");
  return token != "";
}

void IotsaStaticTokenObject::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".token", token);
  cf.put(f_name + ".rights", rights);
}

#ifdef IOTSA_WITH_WEB
void IotsaStaticTokenObject::formHandler_emptyfields(String& message) {
  message += "Token: <input name='token'><br>";
  message += "Rights: <input name='rights'><br>";
}

void IotsaStaticTokenObject::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  IotsaSerial.println("IotsaStaticTokenObject::formHandler not implemented");
}

void IotsaStaticTokenObject::formHandler_TH(String& message, bool includeConfig) {
  message += "<th>Token</th><th>rights</th>";
}

void IotsaStaticTokenObject::formHandler_TD(String& message, bool includeConfig) {
  message += "<td>";
  message += IotsaMod::htmlEncode(token);
  message += "</td><td>";
  message += IotsaMod::htmlEncode(rights);
  message += "</td>";
}

bool IotsaStaticTokenObject::formHandler_args(IotsaWebServer *server, const String& name, bool includeConfig) {
  // name=="" for IotsaUser
  token = server->arg("token");
  rights = server->arg("rights");
  return token != "";
}

#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
void IotsaStaticTokenObject::getHandler(JsonObject& reply) {
  reply["token"] = token;
  reply["rights"] = rights;
}

bool IotsaStaticTokenObject::putHandler(const JsonVariant& request) {
  bool anyChanged;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj["token"].is<const char *>()) {
    token = reqObj["token"].as<String>();
    anyChanged = true;
  }
  if (reqObj["rights"].is<const char *>()) {
    rights = reqObj["rights"].as<String>();
    anyChanged = true;
  }
  return anyChanged;
}
#endif

IotsaStaticTokenMod::IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain)
:	IotsaAuthMod(_app),
	chain(_chain)
{
	configLoad();
}
	
void
IotsaStaticTokenMod::handler() {
  if (needsAuthentication("tokens")) return;
  String command = server->arg("command");

  if (command == "add") {
    IotsaStaticTokenObject newToken;
    if (newToken.formHandler_args(server, "", true)) {
      _addToken(newToken);
      configSave();
    }
  } else if (command == "del") {
    int index = server->arg("index").toInt();
    _delToken(index);
    configSave();
  }


  String message = "<html><head><title>Tokens</title><style>table, th, td {border: 1px solid black;padding:5px;border-collapse: collapse;}</style></head><body><h1>Tokens</h1>";
  message += "<h2>Existing tokens</h2><table><tr>";
  IotsaStaticTokenObject::formHandler_TH(message, true);
  message += "<th>Operation</th></tr>";
  int index=0;
  for(auto t: tokens) {
    message += "<tr>";
    t.formHandler_TD(message, true);
    message += "<td><form><input type='hidden' name='index' value='" + String(index++) + "'><input type='submit' name='command' value='del'></form></td>";
    message += "</tr>";
  }
  message += "</table><br>";

  message += "<h2>Add new token</h2><form method='get'>";
  IotsaStaticTokenObject::formHandler_emptyfields(message);
  message += "<input type='submit' name='command' value='add'>";
  message += "</form><hr>";

  server->send(200, "text/html", message);
}

void IotsaStaticTokenMod::setup() {
  configLoad();
}

void IotsaStaticTokenMod::serverSetup() {
  server->on("/tokens", std::bind(&IotsaStaticTokenMod::handler, this));
}

int IotsaStaticTokenMod::_addToken(IotsaStaticTokenObject& newToken) {
  tokens.push_back(newToken);
  return tokens.size()-1;
}

bool IotsaStaticTokenMod::_delToken(int index) {
  tokens.erase(tokens.begin()+index);
  return true;
}

void IotsaStaticTokenMod::configLoad() {
  IotsaConfigFileLoad cf("/config/statictokens.cfg");
  tokens.clear();
  for(int i=0; ; i++) {
    String f_name = String(i);
    IotsaStaticTokenObject newToken;
    if (!newToken.configLoad(cf, f_name)) break;
    tokens.push_back(newToken);
  }
}

void IotsaStaticTokenMod::configSave() {
  IotsaConfigFileSave cf("/config/statictokens.cfg");
  int i = 0;
  for(auto it: tokens) {
    String f_name = String(i++);
    it.configSave(cf, f_name);
  }
}

void IotsaStaticTokenMod::loop() {
}

#ifdef IOTSA_WITH_WEB
String IotsaStaticTokenMod::info() {
  String message = "<p>Static tokens enabled.";
  message += " See <a href=\"/tokens\">/tokens</a> to change.";
  message += "</p>";
  return message;
}
#endif

bool IotsaStaticTokenMod::allows(const char *right) {
  if (server->hasHeader("Authorization")) {
    String authHeader = server->header("Authorization");
    if (authHeader.startsWith("Bearer ")) {
      String token = authHeader.substring(7);
      String rightField("/");
      rightField += right;
      rightField += "/";
      // Loop over all tokens.
      for(auto t:tokens){
        if (t.token == token) {
          // The token matches. Check the rights.
          if (t.rights == "*" || t.rights.indexOf(rightField) >= 0) {
            return true;
          }
        }
      }
    }
  }
  Serial.println("No token match, try user/password");
  // If no rights fall back to username/password authentication
  return chain.allows(right);
}

bool IotsaStaticTokenMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}

