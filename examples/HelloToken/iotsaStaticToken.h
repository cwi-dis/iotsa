#ifndef _IOTSASTATICTOKEN_H_
#define _IOTSASTATICTOKEN_H_
#include "iotsa.h"

class StaticToken;

class IotsaStaticTokenMod : public IotsaAuthMod {
public:
  IotsaStaticTokenMod(IotsaApplication &_app, IotsaAuthMod &_chain);
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool needsAuthentication(const char *right=NULL);
protected:
  void configLoad();
  void configSave();
  void handler();
  
  IotsaAuthMod &chain;
  int ntoken;
  StaticToken *tokens;
};

#endif
