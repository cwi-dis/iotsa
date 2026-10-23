#include "iotsaConfigFile.h"
#include "iotsaStaticToken.h"

// An authenticator for tokens. A token is a secret that a program sends with
// its request, in an HTTP header:
//
//   Authorization: Bearer <token>
//
// Each token has its own rights, like a user (see HelloUser). It is for a
// program that should be able to do one thing, such as changing the name,
// without knowing a user's password.
//
// Requests without a valid token are passed on to the next authenticator, the
// users (see mainHelloToken.cpp). So people log in as before.
//
// The tokens are managed on the /tokens page. They are a list, so the page uses
// IotsaWebVector, which gives each token its own row plus Add and Remove.

//
// One token.
//
bool IotsaStaticTokenObject::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  cf.get(f_name + ".token", token, "");
  cf.get(f_name + ".rights", rights, "");
  return token != "";
}

void IotsaStaticTokenObject::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".token", token);
  cf.put(f_name + ".rights", rights);
}

void IotsaStaticTokenObject::putHandler(IotsaApiContext& ctx) {
  ctx.argPassword("token", token, IotsaApiContext::Saved);
  ctx.arg("rights", rights, IotsaApiContext::Saved);
}

// The token itself is a secret: only whether it is set.
void IotsaStaticTokenObject::getHandler(IotsaApiContext& ctx) {
  ctx.show("rights", rights);
  ctx.showsNeedRight(IOTSA_RIGHT_NEVER);
  ctx.show("token", token);
}

#ifdef IOTSA_WITH_WEB
void IotsaStaticTokenObject::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("token", token, IotsaWebContext::Saved);
  ctx.arg("rights", rights, IotsaWebContext::Saved);
}

// The fields for adding a new token.
void IotsaStaticTokenObject::formHandler_fields(IotsaWebContext& ctx) {
  ctx.showField("Token", "token", token, IotsaWebContext::Saved);
  ctx.showField("Rights", "rights", rights, IotsaWebContext::Saved, "* for all, or /right/right/");
}

// One row of the table of existing tokens. The token is a secret: the page
// only shows that it is there.
void IotsaStaticTokenObject::formHandler_row(IotsaWebContext& ctx) {
  ctx.showCell("Token", token, IotsaWebContext::Secret);
  ctx.showCell("Rights", rights);
}
#endif // IOTSA_WITH_WEB

//
// The module.
//
IotsaStaticTokenMod::IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain)
:	IotsaAuthMod(_app),
	chain(_chain)
#ifdef IOTSA_WITH_WEB
  , webTokens(tokens, IotsaWebCollection::Add | IotsaWebCollection::Remove, IotsaWebCollection::Table, IotsaWebContext::Saved,
      nullptr,
      // Refuse a new token that is empty.
      [](IotsaStaticTokenObject& t, IotsaWebContext& ctx) {
        if (t.token == "") ctx.error("A new token can't be empty.");
        return t.token != "";
      })
#endif
{
}

void IotsaStaticTokenMod::setup() {
  configLoad();
}

void IotsaStaticTokenMod::lateSetup() {
  // Only GET: the tokens are changed on the web page.
  api.setup("tokens", true);
  name = "tokens";
}

void IotsaStaticTokenMod::loop() {
}

// May this request do something that needs the right "right"? Yes if it
// carries a token that has the right; otherwise ask the users.
IotsaAccess IotsaStaticTokenMod::allows(IotsaContext& ctx, const char *right) {
  String token = ctx.bearerToken();
  if (token != "") {
    String rightField("/");
    rightField += right;
    rightField += "/";
    for (auto& t : tokens) {
      if (t.token == token) {
        if (t.rights == "*" || t.rights.indexOf(rightField) >= 0) {
          return IotsaAccess::Granted;
        }
        // A known token without the right: the users may still have it, but
        // logging in won't help the token.
        IotsaAccess access = chain.allows(ctx, right);
        return access == IotsaAccess::NeedsCredentials ? IotsaAccess::Forbidden : access;
      }
    }
  }
  return chain.allows(ctx, right);
}

// The tokens are kept in a file, so they survive a reboot.
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
  for(auto& it: tokens) {
    String f_name = String(i++);
    it.configSave(cf, f_name);
  }
}

// The tokens themselves are secrets: only their rights are sent.
void IotsaStaticTokenMod::getHandler(IotsaApiContext& ctx) {
  JsonArray list = ctx.reply()["tokens"].to<JsonArray>();
  int i = 0;
  for (auto& t : tokens) {
    IotsaApiContext::Scope s(ctx, String(i++), JsonObject(), list.add<JsonObject>());
    t.getHandler(ctx);
  }
}

#ifdef IOTSA_WITH_WEB
void IotsaStaticTokenMod::formHandler_args(IotsaWebContext& ctx) {
  // Changing the tokens needs the right "tokens".
  ctx.argsNeedRight("tokens");
  IotsaWebContext::Scope s(ctx, "tokens");
  webTokens.formHandler_args(ctx);
}

void IotsaStaticTokenMod::formHandler_fields(IotsaWebContext& ctx) {
  ctx.argsNeedRight("tokens");
  // The tokens are configuration, there is no everyday part.
  if (!ctx.includesSettings()) return;
  IotsaWebContext::Scope s(ctx, "tokens");
  webTokens.formHandler_fields(ctx);
}

String IotsaStaticTokenMod::info() {
  String message = "<p>Static tokens enabled.";
  message += " See <a href=\"/tokens\">/tokens</a> to change.";
  message += "</p>";
  return message;
}
#endif
