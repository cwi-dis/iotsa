#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"
#include "iotsaApi.h"

//
// Tokens: secrets that a program (rather than a person) sends with its
// requests, each with its own rights. See iotsaStaticToken.cpp.
//

// One token and its rights.
class IotsaStaticTokenObject : public IotsaApiModObject {
public:
  IotsaStaticTokenObject() {}
  String token;
  String rights;
public:
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  void formHandler_row(IotsaWebContext& ctx) override;
#endif
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
};

// The authenticator: the list of tokens, and the users to ask when a request
// has no valid token.
// The tokens, as a table with Add and Remove on the web page, and in the API
// (cwi-dis/iotsa#309). Keyed by index (a token is a secret, not a name): the
// list's version makes a stale Remove on the page harmless.
class IotsaStaticTokenList : public IotsaVector<IotsaStaticTokenObject> {
public:
  IotsaStaticTokenList(std::vector<IotsaStaticTokenObject>& tokens)
  : IotsaVector<IotsaStaticTokenObject>("tokens", tokens, Add | Remove, Table, IotsaContext::Saved)
  {}
protected:
  bool accept(IotsaStaticTokenObject& token, IotsaContext& ctx) override;
};

class IotsaStaticTokenMod : public IotsaAuthMod {
public:
  IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain);
  void setup() override;
  void lateSetup() override;
  void loop() override;
  IotsaAccess allows(IotsaContext& ctx, const char *right) override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  void configLoad() override;
  void configSave() override;
  void getHandler(IotsaApiContext& ctx) override;
  void putHandler(IotsaApiContext& ctx) override;
  void postHandler(IotsaApiContext& ctx) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Tokens"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif

  IotsaAuthenticationProvider &chain;
  std::vector<IotsaStaticTokenObject> tokens;
  IotsaStaticTokenList tokenList{tokens};
};

#endif
