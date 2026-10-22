#include "iotsa.h"
#include "iotsaCapabilities.h"
#include "iotsaConfigFile.h"
#include <cmArduinoJWT.h>
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
  chain(_chain),
  trustedIssuer(""),
  issuerKey("")
{
	configLoad();
}

void IotsaCapabilityMod::setup() {
  configLoad();
}

void IotsaCapabilityMod::lateSetup() {
  api.setup("capabilities", true, true, false);
  name = "capabilities";
}

void IotsaCapabilityMod::loop() {
}

IotsaAccess IotsaCapabilityMod::allows(IotsaContext& ctx, const char *right) {
  // Capabilities are about API paths and verbs: only the REST rule.
  if (right == nullptr) {
    loadCapabilitiesFromRequest(ctx);
    if (capabilities) {
      const char *obj = ctx.apiPath().c_str();
      if (capabilities->allows(obj, ctx.verb())) {
        IFDEBUGX IotsaSerial.print("Capability allows operation on ");
        IFDEBUGX IotsaSerial.println(obj);
        return IotsaAccess::Granted;
      }
      IFDEBUGX IotsaSerial.print("Capability does NOT allow operation on ");
      IFDEBUGX IotsaSerial.println(obj);
    }
  }
  // Otherwise fall back to username/password authentication
  return chain.allows(ctx, right);
}

void IotsaCapabilityMod::loadCapabilitiesFromRequest(IotsaContext& ctx) {

  // Free old capabilities
  IotsaCapability **cpp;
  for (cpp=&capabilities; *cpp; cpp=&(*cpp)->next) free(*cpp);
  capabilities = NULL;

  // Check that we can load and verify capabilities
  if (trustedIssuer == "" || issuerKey == "") return;

  // Load the bearer token from the request.
  String token = ctx.bearerToken();
  if (token == "") {
    IFDEBUGX Serial.println("No bearer token in request");
    return;
  }

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

void IotsaCapabilityMod::putHandler(IotsaApiContext& ctx) {
  ctx.argsNeedRight("capabilities");
  ctx.arg("trustedIssuer", trustedIssuer, IotsaApiContext::SavedInConfigMode);
  ctx.argPassword("issuerKey", issuerKey, IotsaApiContext::SavedInConfigMode);
}


void IotsaCapabilityMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("trustedIssuer", trustedIssuer);
  ctx.showsNeedRight(IOTSA_RIGHT_NEVER);
  ctx.show("issuerKey", issuerKey);
}

#ifdef IOTSA_WITH_WEB
// The trust anchors: only in configuration mode, like users and WiFi.
void IotsaCapabilityMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.argsNeedRight("capabilities");
  ctx.arg("trustedIssuer", trustedIssuer, IotsaWebContext::SavedInConfigMode);
  ctx.argPassword("issuerKey", issuerKey, IotsaWebContext::SavedInConfigMode);
}

void IotsaCapabilityMod::formHandler_fields(IotsaWebContext& ctx) {
  ctx.argsNeedRight("capabilities");
  if (!ctx.includesSettings()) return;
  IotsaWebContext::Form form(ctx);
  ctx.showField("Issuer", "trustedIssuer", trustedIssuer, IotsaWebContext::SavedInConfigMode);
  ctx.showStatus("Shared key", "has_issuerKey", issuerKey.length() ? "set" : "not set");
  ctx.showPassword("New shared key", "issuerKey", IotsaWebContext::SavedInConfigMode, "empty: unchanged");
}

String IotsaCapabilityMod::info() {
  String message = "<p>Capabilities enabled.";
  message += " See <a href=\"/capabilities\">/capabilities</a> to change settings.";
  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB
