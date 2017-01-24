#ifndef _IOTSASIMPLEIO_H_
#define _IOTSASIMPLEIO_H_
#include "iotsa.h"

class IotsaSimpleIOMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
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
