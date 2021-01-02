#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"

class IotsaStaticTokenObject : IotsaModObject {
public:
  IotsaStaticTokenObject() {}
  String token;
  String rights;
public:
  bool configLoad(IotsaConfigFileLoad& cf, String& f_name);
  void configSave(IotsaConfigFileSave& cf, String& f_name);
#ifdef IOTSA_WITH_WEB
  static void formHandler(String& message);
  void formHandler(String& message, String& text, String& f_name);
  static void formHandlerTH(String& message);
  void formHandlerTD(String& message);
  bool formArgHandler(IotsaWebServer *server, String name);
#endif
#ifdef IOTSA_WITH_API
  void getHandler(JsonObject& reply);
  bool putHandler(const JsonVariant& request);
#endif
};

class IotsaStaticTokenMod : public IotsaAuthMod {
public:
  IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain);
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool allows(const char *right=NULL);
  bool allows(const char *obj, IotsaApiOperation verb);
protected:
  void configLoad();
  void configSave();
  void handler();
  int _addToken(IotsaStaticTokenObject& newToken);
  bool _delToken(int index);
  
  IotsaAuthenticationProvider &chain;
  std::vector<IotsaStaticTokenObject> tokens;
};

#endif
