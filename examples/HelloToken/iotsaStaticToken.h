#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"

class IotsaStaticTokenObject : IotsaModObject {
public:
  IotsaStaticTokenObject() {}
  String token;
  String rights;
public:
  bool configLoad(IotsaConfigFileLoad& cf, String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, String& f_name) override;
#ifdef IOTSA_WITH_WEB
  static void formHandler(String& message) /*override*/;
  void formHandler(String& message, String& text, String& f_name) override;
  static void formHandlerTH(String& message) /* override*/;
  void formHandlerTD(String& message) override;
  bool formArgHandler(IotsaWebServer *server, String name) override;
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
  String info() override;
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
