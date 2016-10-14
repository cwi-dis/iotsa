#ifndef _IOTSANOTHING_H_
#define _IOTSANOTHING_H_
#include "iotsa.h"

class IotsaNothingMod : public IotsaMod {
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
