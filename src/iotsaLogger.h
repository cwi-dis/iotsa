#ifndef _IOTSALOGGER_H_
#define _IOTSALOGGER_H_
#include "iotsa.h"

class IotsaLoggerMod : public IotsaMod {
public:
  IotsaLoggerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL);
  void setup();
  void serverSetup();
  void loop();
  String info();
protected:
  void configLoad();
  void configSave();
  void handler();
  String argument;
};

#endif
