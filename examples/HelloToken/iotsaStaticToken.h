#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"

class StaticToken;

class IotsaStaticTokenMod : public IotsaAuthMod {
public:
  IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain);
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool allows(const char *right=NULL);
  bool allows(const char *obj, const char *verb);
protected:
  void configLoad();
  void configSave();
  void handler();
  
  IotsaAuthenticationProvider &chain;
  int ntoken;
  StaticToken *tokens;
};

#endif
