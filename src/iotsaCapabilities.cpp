#include "iotsa.h"
#include "iotsaCapabilities.h"
#include "iotsaConfigFile.h"
#include <ArduinoJWT.h>
#include <ArduinoJson.h>

#define IFDEBUGX if(1)

#if 0
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
  for(size_t i=0; i<gotArray.size(); i++) {
    const char *gotItem = gotArray[i];
    if (strcmp(gotItem, wanted) == 0) {
      return true;
    }
  }
  return false;
}
#endif

// Get a scope indicator from a JSON variant
static IotsaCapabilityObjectScope getRightFrom(const JsonVariant& arg) {
  if (!arg.is<const char*>()) return IOTSA_SCOPE_NONE;
  const char *argStr = arg.as<const char*>();
  if (strcmp(argStr, "self") == 0) return IOTSA_SCOPE_SELF;
  if (strcmp(argStr, "descendant-or-self") == 0) return IOTSA_SCOPE_FULL;
  if (strcmp(argStr, "descendant") == 0) return IOTSA_SCOPE_CHILD;
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
#ifdef IOTSA_WITH_API
  api(this, _app, this),
#endif
  chain(_chain),
  trustedIssuer(""),
  issuerKey("")
{
	configLoad();
}

#ifdef IOTSA_WITH_WEB
void
IotsaCapabilityMod::handler() {
  String _trustedIssuer = server->arg("trustedIssuer");
  String _issuerKey = server->arg("issuerKey");
  if (_trustedIssuer != "" || _issuerKey != "") {
    if (!iotsaConfig.inConfigurationMode(true)) {
      server->send(403, "text/plain", "403 Forbidden, not in configuration mode");
      return;
    }
    if (needsAuthentication("capabilities")) return;
    if (_trustedIssuer != "") trustedIssuer = _trustedIssuer;
    if (_issuerKey != "") issuerKey = _issuerKey;
    configSave();
    server->send(200, "text/plain", "ok\r\n");
    return;
  }
  String message = "<html><head><title>Capability Authority</title></head><body><h1>Capability Authority</h1>";
  if (!iotsaConfig.inConfigurationMode())
    message += "<p><em>Note:</em> You must be in configuration mode to be able to change issuer or key.</p>";
  message += "<form method='get'>Issuer: <input name='trustedIssuer' value='";
  message += htmlEncode(trustedIssuer);
  message += "'><br>Current shared key is secret, but length is ";
  message += String(issuerKey.length());
  message += ".<br>New key: <input name='issuerKey'><br><input type='Submit'></form></body></html>";

  server->send(200, "text/html", message);
}

String IotsaCapabilityMod::info() {
  String message = "<p>Capabilities enabled.";
  message += " See <a href=\"/capabilities\">/capabilities</a> to change settings.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaCapabilityMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/capabilities") != 0) return false;
  reply["trustedIssuer"] = trustedIssuer;
  reply["has_issuerKey"] = (issuerKey.length() > 0);
  return true;
}

bool IotsaCapabilityMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (strcmp(path, "/api/capabilities") != 0) return false;
  if (!iotsaConfig.inConfigurationMode()) return false;
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "trustedIssuer", trustedIssuer)) {
    anyChanged = true;
  }
  if (getFromRequest<const char *>(reqObj, "issuerKey", issuerKey)) {
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
#endif // IOTSA_WITH_API

void IotsaCapabilityMod::setup() {
  configLoad();
}

void IotsaCapabilityMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/capabilities", std::bind(&IotsaCapabilityMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/capabilities", true, true, false);
  name = "capabilities";
#endif
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

bool IotsaCapabilityMod::allows(const char *obj, IotsaApiOperation verb) {
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  loadCapabilitiesFromRequest();
#endif
#ifdef IOTSA_WITH_COAP
  // Need to load capability from coap headers, somehow...
#endif
#ifdef IOTSA_WITH_HPS
  // Need to load capabilit from hps headers.
#endif
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
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
void IotsaCapabilityMod::loadCapabilitiesFromRequest() {

  // Free old capabilities
  IotsaCapability **cpp;
  for (cpp=&capabilities; *cpp; cpp=&(*cpp)->next) free(*cpp);
  capabilities = NULL;

  // Check that we can load and verify capabilities
  if (trustedIssuer == "" || issuerKey == "") return;

  // Load the bearer token from the request
  if (!server->hasHeader("Authorization")) {
    IFDEBUGX Serial.println("No authorization header in request");
    return;
  }
  String authHeader = server->header("Authorization");
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
  JsonDocument jsonDocument;
  deserializeJson(jsonDocument, payload);
  JsonObject root = jsonDocument.to<JsonObject>();
  
  // check that issuer matches
  String issuer = root["iss"];
  if (issuer != trustedIssuer) {
    IFDEBUGX IotsaSerial.print("Issuer did not match, wtd=");
    IFDEBUGX IotsaSerial.print(trustedIssuer);
    IFDEBUGX IotsaSerial.print(", got=");
    IFDEBUGX IotsaSerial.println(issuer);
    return;
  }

  if (root["aud"].is<const char *>()) {
    String audience = root["aud"].as<String>();
    String myFullName = iotsaConfig.hostName + ".local";
#ifdef IOTSA_WITH_HTTPS
    String myUrl = "https://" + myFullName;
#else
    String myUrl = "http://" + myFullName;
#endif
    if (audience != "*" && audience != myFullName && audience != myUrl) {
      IFDEBUGX IotsaSerial.print("Audience did not match, wtd=");
      IFDEBUGX IotsaSerial.print(myFullName);
      IFDEBUGX IotsaSerial.print(", got=");
      IFDEBUGX IotsaSerial.println(audience);
      return;
    }
  }
  const char *obj = root["obj"];
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
#endif // IOTSA_WITH_HTTP_OR_HTTPS