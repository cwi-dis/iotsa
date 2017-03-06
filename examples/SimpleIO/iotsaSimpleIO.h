#ifndef _IOTSASIMPLEIO_H_
#define _IOTSASIMPLEIO_H_
#include "iotsa.h"

class GPIOPort {
public:
  GPIOPort(const char* _name, int _pin)
  : name(_name),
    pin(_pin),
    mode(INPUT),
    value(false)
  {}
  void update();

  String name;
  int pin;
  int mode;
  bool value;
};

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
