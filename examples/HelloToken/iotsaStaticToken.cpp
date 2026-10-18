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
void IotsaStaticTokenObject::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("token", token);
  ctx.arg("rights", rights);
}

void IotsaStaticTokenObject::formHandler_fields(IotsaWebContext& ctx) {
  ctx.field("Token", "token", token);
  ctx.field("Rights", "rights", rights, IotsaWebContext::Config, "* for all, or /right/right/");
}

void IotsaStaticTokenObject::formHandler_row(IotsaWebContext& ctx) {
  // The token is a bearer secret: the page only shows that it is there.
  ctx.cell("Token", token, IotsaWebContext::Secret);
  ctx.cell("Rights", rights);
}
#endif // IOTSA_WITH_WEB

bool IotsaStaticTokenObject::getHandler(JsonObject& reply) {
  reply["token"] = token;
  reply["rights"] = rights;
  return true;
}

bool IotsaStaticTokenObject::putHandler(const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  JsonObject reqObj = request.as<JsonObject>();
  bool any = false;
  any |= getFromRequest<String>(reqObj, "token", token);
  any |= getFromRequest<String>(reqObj, "rights", rights);
  if (any && changed) *changed = true;
  return true;
}

IotsaStaticTokenMod::IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain)
:	IotsaAuthMod(_app),
	chain(_chain)
#ifdef IOTSA_WITH_WEB
  , webTokens(tokens, IotsaWebCollection::Add | IotsaWebCollection::Remove, IotsaWebCollection::Table, IotsaWebContext::Config,
      nullptr,
      [](IotsaStaticTokenObject& t, IotsaWebContext& ctx) {
        if (t.token == "") ctx.error("A new token can't be empty.");
        return t.token != "";
      })
#endif
{
	configLoad();
}
	
#ifdef IOTSA_WITH_WEB
void IotsaStaticTokenMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.needsRight("tokens");
  IotsaWebContext::Scope s(ctx, "tokens");
  webTokens.formHandler_args(ctx);
}

void IotsaStaticTokenMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Configuration) return;
  IotsaWebContext::Scope s(ctx, "tokens");
  webTokens.formHandler_fields(ctx);
}
#endif

// The tokens themselves are secrets: only their rights.
bool IotsaStaticTokenMod::getHandler(const char *path, JsonObject& reply) {
  JsonArray list = reply["tokens"].to<JsonArray>();
  for (auto& t : tokens) {
    JsonObject o = list.add<JsonObject>();
    o["rights"] = t.rights;
  }
  return true;
}

void IotsaStaticTokenMod::setup() {
  configLoad();
}

void IotsaStaticTokenMod::lateSetup() {
  api.setup("tokens", true);
  name = "tokens";
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
  if (app.server->hasHeader("Authorization")) {
    String authHeader = app.server->header("Authorization");
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

