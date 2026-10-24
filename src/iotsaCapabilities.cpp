#include "iotsa.h"
#include "iotsaCapabilities.h"
#include "iotsaConfigFile.h"
#include <cmArduinoJWT.h>
#include <ArduinoJson.h>

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
  // Capabilities are about API paths and verbs, the REST rule (right ==
  // nullptr). That rule is gone, so this is never taken any more: a
  // capability grants nothing until it can name rights (cwi-dis/iotsa#107,
  // #304; https://github.com/cwi-dis/iotsa/issues/183#issuecomment-6368033789).
  if (right == nullptr) {
    loadCapabilitiesFromRequest(ctx);
    if (capabilities) {
      const char *obj = ctx.apiPath().c_str();
      if (capabilities->allows(obj, ctx.verb())) {
        IOTSA_LOG_DEBUG("iotsaCapabilities", "allows operation on %s", obj);
        return IotsaAccess::Granted;
      }
      IOTSA_LOG("iotsaCapabilities", "does not allow operation on %s", obj);
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
    IOTSA_LOG_DEBUG("iotsaCapabilities", "no bearer token in request");
    return;
  }

  // Decode the bearer token
  ArduinoJWT decoder(issuerKey);
  String payload;
  bool ok = decoder.decodeJWT(token, payload);
  // If decode returned false the token wasn't signed with the correct key.
  if (!ok) {
    IOTSA_LOG("iotsaCapabilities", "token did not decode correctly with key");
    return;
  }
  JsonDocument jsonDocument;
  deserializeJson(jsonDocument, payload);
  JsonObject root = jsonDocument.to<JsonObject>();
  
  // check that issuer matches
  String issuer = root["iss"];
  if (issuer != trustedIssuer) {
    IOTSA_LOG("iotsaCapabilities", "issuer did not match, wanted %s, got %s", trustedIssuer.c_str(), issuer.c_str());
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
      IOTSA_LOG("iotsaCapabilities", "audience did not match, wanted %s, got %s", myFullName.c_str(), audience.c_str());
      return;
    }
  }
  const char *obj = root["obj"];
  IotsaCapabilityObjectScope get = getRightFrom(root["get"]);
  IotsaCapabilityObjectScope put = getRightFrom(root["put"]);
  IotsaCapabilityObjectScope post = getRightFrom(root["post"]);
  IOTSA_LOG_DEBUG("iotsaCapabilities", "capability for %s loaded, get=%d, put=%d, post=%d", obj, int(get), int(put), int(post));
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
  IOTSA_LOG("iotsaCapabilities", "saved capabilities.cfg, issuer=%s, key length=%u", trustedIssuer.c_str(), (unsigned)issuerKey.length());
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
