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
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Tokens"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif

  IotsaAuthenticationProvider &chain;
  std::vector<IotsaStaticTokenObject> tokens;
#ifdef IOTSA_WITH_WEB
  // Keyed by index: the collection's version makes a stale Remove harmless.
  IotsaWebVector<IotsaStaticTokenObject> webTokens;
#endif
};

#endif
