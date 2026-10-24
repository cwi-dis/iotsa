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
// The tokens are managed on the /tokens page and through /api/tokens. They are
// a list, so the module uses IotsaStaticTokenList, a subclass of IotsaVector,
// which gives each token its own row plus Add and Remove on the page, and the
// collection's shape in the API (cwi-dis/iotsa#309); the subclass only says
// which new tokens to refuse.

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
{
}

// Refuse a new token that is empty.
bool IotsaStaticTokenList::accept(IotsaStaticTokenObject& token, IotsaContext& ctx) {
  if (token.token == "") {
    ctx.error(400, "A new token can't be empty.");
    return false;
  }
  return true;
}

void IotsaStaticTokenMod::setup() {
  configLoad();
}

void IotsaStaticTokenMod::lateSetup() {
  api.setup("tokens", true, true, true);
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

// The tokens as a collection (cwi-dis/iotsa#309). The tokens themselves are
// secrets: only their rights are sent. "tokens": [...] replaces all,
// "tokens": {"0": {...}} changes the first, "0": null removes it; a POST of
// "tokens": {...} adds one. Changes need the right "tokens", as on the page.
void IotsaStaticTokenMod::getHandler(IotsaApiContext& ctx) {
  tokenList.getHandler(ctx);
}

void IotsaStaticTokenMod::putHandler(IotsaApiContext& ctx) {
  ctx.argsNeedRight("tokens");
  tokenList.putHandler(ctx);
}

void IotsaStaticTokenMod::postHandler(IotsaApiContext& ctx) {
  ctx.argsNeedRight("tokens");
  tokenList.postHandler(ctx);
}

#ifdef IOTSA_WITH_WEB
void IotsaStaticTokenMod::formHandler_args(IotsaWebContext& ctx) {
  // Changing the tokens needs the right "tokens".
  ctx.argsNeedRight("tokens");
  IotsaWebContext::Scope s(ctx, "tokens");
  tokenList.formHandler_args(ctx);
}

void IotsaStaticTokenMod::formHandler_fields(IotsaWebContext& ctx) {
  ctx.argsNeedRight("tokens");
  // The tokens are configuration, there is no everyday part.
  if (!ctx.includesSettings()) return;
  IotsaWebContext::Scope s(ctx, "tokens");
  tokenList.formHandler_fields(ctx);
}

String IotsaStaticTokenMod::info() {
  String message = "<p>Static tokens enabled.";
  message += " See <a href=\"/tokens\">/tokens</a> to change.";
  message += "</p>";
  return message;
}
#endif
