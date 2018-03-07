#include "iotsa.h"
#include "iotsaCapabilities.h"
#include "iotsaConfigFile.h"
#include <ArduinoJWT.h>
#include <ArduinoJson.h>

#define IFDEBUGX if(1)

// Static method to check whether a string exactly matches a Json object,
// or is included in the Json object if it is an array.
static bool stringContainedIn(const char *wanted, JsonVariant& got) {
  if (got.is<char*>()) {
    return strcmp(wanted, got.as<const char *>()) == 0;
  }
  if (!got.is<JsonArray>()) {
    return false;
  }
  JsonArray& gotArray = got.as<JsonArray>();
  for(int i=0; i<gotArray.size(); i++) {
    const char *gotItem = gotArray[i];
    if (strcmp(gotItem, wanted) == 0) {
      return true;
    }
  }
  return false;
}

// Get a scope indicator from a JSON variant
static IotsaCapabilityObjectScope getRightFrom(const JsonVariant& arg) {
  if (!arg.is<char*>()) return IOTSA_SCOPE_NONE;
  const char *argStr = arg.as<char*>();
  if (strcmp(argStr, "self") == 0) return IOTSA_SCOPE_SELF;
  if (strcmp(argStr, "descendent-or-self") == 0) return IOTSA_SCOPE_FULL;
  if (strcmp(argStr, "descendent") == 0) return IOTSA_SCOPE_CHILD;
  if (strcmp(argStr, "child") == 0) return IOTSA_SCOPE_CHILD;
  return IOTSA_SCOPE_NONE;
}

bool IotsaCapability::allows(const char *_obj, IotsaApiOperation verb) {
  IotsaCapabilityObjectScope scope = scopes[int(verb)];
  int matchLength = obj.length();
  switch(scope) {
    case IOTSA_SCOPE_NONE:
      break;
    case IOTSA_SCOPE_SELF:
      if (strcmp(obj.c_str(), _obj) == 0)
        return true;
      break;
    case IOTSA_SCOPE_FULL:
      if (strncmp(obj.c_str(), _obj, matchLength) == 0) {
        char nextCh = _obj[matchLength];
        if (nextCh == '\0' || nextCh == '/')
          return true;
      }
      break;
    case IOTSA_SCOPE_CHILD:
      if (strncmp(obj.c_str(), _obj, matchLength) == 0) {
        char nextCh = _obj[matchLength];
        if (nextCh == '/')
          return true;
      }
      break;
  }
  // See if there is a next capabiliy we can check, otherwise we don't have permission.
  if (next) return next->allows(_obj, verb);
  return false;
}

IotsaCapabilityMod::IotsaCapabilityMod(IotsaApplication &_app, IotsaAuthenticationProvider& _chain)
:	IotsaAuthMod(_app),
  capabilities(NULL),
  api(this, this, server),
  chain(_chain),
  trustedIssuer(""),
  issuerKey("")
{
	configLoad();
}
	
void
IotsaCapabilityMod::handler() {
  String _trustedIssuer = server.arg("trustedIssuer");
  String _issuerKey = server.arg("issuerKey");
  if (_trustedIssuer != "" || _issuerKey != "") {
    if (configurationMode != TMPC_CONFIG) {
      server.send(401, "text/plain", "401 Unauthorized, not in configuration mode");
      return;
    }
    if (needsAuthentication("capabilities")) return;
    if (_trustedIssuer != "") trustedIssuer = _trustedIssuer;
    if (_issuerKey != "") issuerKey = _issuerKey;
    configSave();
    server.send(200, "text/plain", "ok\r\n");
    return;
  }
  String message = "<html><head><title>Capability Authority</title></head><body><h1>Capability Authority</h1>";
  if (configurationMode != TMPC_CONFIG)
    message += "<p><em>Note:</em> You must be in configuration mode to be able to change issuer or key.</p>";
  message += "<form method='get'>Issuer: <input name='trustedIssuer' value='";
  message += htmlEncode(trustedIssuer);
  message += "'><br>Current shared key is secret, but length is ";
  message += String(issuerKey.length());
  message += ".<br>New key: <input name='issuerKey'><br><input type='Submit'></form></body></html>";

  server.send(200, "text/html", message);
}

bool IotsaCapabilityMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/capabilities") != 0) return false;
  reply["trustedIssuer"] = trustedIssuer;
  reply["issuerKeyLength"] = issuerKey.length();
  return true;
}

bool IotsaCapabilityMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/capabilities") != 0) return false;
  if (configurationMode != TMPC_CONFIG) return false;
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("trustedIssuer")) {
    trustedIssuer = reqObj.get<String>("trustedIssuer");
    anyChanged = true;
  }
  if (reqObj.containsKey("issuerKey")) {
    issuerKey = reqObj.get<String>("issuerKey");
    anyChanged = true;
  }
  if (anyChanged) {
    configSave();
  }
  return anyChanged;
}

bool IotsaCapabilityMod::postHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  return false;
}

void IotsaCapabilityMod::setup() {
  configLoad();
}

void IotsaCapabilityMod::serverSetup() {
  server.on("/capabilities", std::bind(&IotsaCapabilityMod::handler, this));
  api.setup("/api/capabilities", true, true, false);
}

void IotsaCapabilityMod::configLoad() {
  IotsaConfigFileLoad cf("/config/capabilities.cfg");
  cf.get("issuer", trustedIssuer, "");
  cf.get("key", issuerKey, "");
}

void IotsaCapabilityMod::configSave() {
  IotsaConfigFileSave cf("/config/capabilities.cfg");
  cf.put("issuer", trustedIssuer);
  cf.put("key", issuerKey);
  IotsaSerial.print("Saved capabilities.cfg, issuer=");
  IotsaSerial.print(trustedIssuer);
  IotsaSerial.print(", key length=");
  IotsaSerial.println(issuerKey.length());
}

void IotsaCapabilityMod::loop() {
}

String IotsaCapabilityMod::info() {
  String message = "<p>Capabilities enabled.";
  message += " See <a href=\"/capabilities\">/capabilities</a> to change settings.";
  message += "</p>";
  return message;
}

bool IotsaCapabilityMod::allows(const char *obj, IotsaApiOperation verb) {
  loadCapabilitiesFromRequest();
  if (capabilities) {
    if (capabilities->allows(obj, verb)) {
      IFDEBUGX IotsaSerial.print("Capability allows operation on ");
      IFDEBUGX IotsaSerial.println(obj);
      return true;
    }
    IFDEBUGX IotsaSerial.print("Capability does NOT allow operation on ");
    IFDEBUGX IotsaSerial.println(obj);
  }
  // If no rights fall back to username/password authentication
  return chain.allows(obj, verb);
}

bool IotsaCapabilityMod::allows(const char *right) {
  // If no rights fall back to username/password authentication
  return chain.allows(right);
}

void IotsaCapabilityMod::loadCapabilitiesFromRequest() {

  // Free old capabilities
  IotsaCapability **cpp;
  for (cpp=&capabilities; *cpp; cpp=&(*cpp)->next) free(*cpp);
  capabilities = NULL;

  // Check that we can load and verify capabilities
  if (trustedIssuer == "" || issuerKey == "") return;

  // Load the bearer token from the request
  if (!server.hasHeader("Authorization")) {
    IFDEBUGX Serial.println("No authorization header in request");
    return;
  }
  String authHeader = server.header("Authorization");
  if (!authHeader.startsWith("Bearer ")) {
    IFDEBUGX Serial.println("No bearer token in request");
    return;
  }
  String token = authHeader.substring(7);

  // Decode the bearer token
  ArduinoJWT decoder(issuerKey);
  String payload;
  bool ok = decoder.decodeJWT(token, payload);
  // If decode returned false the token wasn't signed with the correct key.
  if (!ok) {
    IFDEBUGX IotsaSerial.println("Did not decode correctly with key");
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);
  
  // check that issuer matches
  String issuer = root["iss"];
  if (issuer != trustedIssuer) {
    IFDEBUGX IotsaSerial.print("Issuer did not match, wtd=");
    IFDEBUGX IotsaSerial.print(trustedIssuer);
    IFDEBUGX IotsaSerial.print(", got=");
    IFDEBUGX IotsaSerial.println(issuer);
    return;
  }

  // Check that the audience matches
  if (root.containsKey("aud")) {
    JsonVariant audience = root["aud"];
    String myUrl("http://");
    myUrl += hostName;
    myUrl += ".local";
    if (audience != "*" && !stringContainedIn(myUrl.c_str(), audience)) {
      IFDEBUGX IotsaSerial.print("Audience did not match, wtd=");
      IFDEBUGX IotsaSerial.print(myUrl);
      IFDEBUGX IotsaSerial.print(", got=");
      IFDEBUGX IotsaSerial.println(audience.as<String>());
      return;
    }
  }
  const char *obj = root.get<char*>("obj");
  IotsaCapabilityObjectScope get = getRightFrom(root["get"]);
  IotsaCapabilityObjectScope put = getRightFrom(root["put"]);
  IotsaCapabilityObjectScope post = getRightFrom(root["post"]);
  IFDEBUGX IotsaSerial.print("capability for ");
  IFDEBUGX IotsaSerial.print(obj);
  IFDEBUGX IotsaSerial.print(", get=");
  IFDEBUGX IotsaSerial.print(int(get));
  IFDEBUGX IotsaSerial.print(", put=");
  IFDEBUGX IotsaSerial.print(int(put));
  IFDEBUGX IotsaSerial.print(", post=");
  IFDEBUGX IotsaSerial.print(int(post));
  IFDEBUGX IotsaSerial.println(" loaded");
  capabilities = new IotsaCapability(obj, get, put, post);
}