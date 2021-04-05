#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"

class IotsaStaticTokenObject : IotsaApiModObject {
public:
  IotsaStaticTokenObject() {}
  String token;
  String rights;
public:
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
#ifdef IOTSA_WITH_WEB
  static void formHandler_new(String& message) /*override*/;
  void formHandler_body(String& message, const String& text, const String& f_name, bool includeConfig) override;
  static void formHandler_TH(String& message, bool includeConfig) /* override*/;
  void formHandler_TD(String& message, bool includeConfig) override;
  bool formHandler_args(IotsaWebServer *server, const String& name, bool includeConfig) override;
#endif
#ifdef IOTSA_WITH_API
  void getHandler(JsonObject& reply) override;
  bool putHandler(const JsonVariant& request) override;
#endif
};

class IotsaStaticTokenMod : public IotsaAuthMod {
public:
  IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain);
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
protected:
  void configLoad() override;
  void configSave() override;
  void handler();
  int _addToken(IotsaStaticTokenObject& newToken);
  bool _delToken(int index);
  
  IotsaAuthenticationProvider &chain;
  std::vector<IotsaStaticTokenObject> tokens;
};

#endif
