#ifndef _IOTSASIMPLEIO_H_
#define _IOTSASIMPLEIO_H_
#include "iotsa.h"

class GPIOPort {
public:
  GPIOPort(const char* _name, int _pin)
  : name(_name),
    pin(_pin),
    mode(INPUT),
    value(false),
    changed(false)
  {}
  void update();
  void setMode(int _mode) {
    mode = _mode;
    pinMode(pin, mode);
  };
  void setValue(bool _value) {
    value = _value;
    if (mode == OUTPUT)
      digitalWrite(pin, value ? HIGH : LOW);
  };
  int getValue() {
    if (mode == OUTPUT) return value;
    return digitalRead(pin);
  };
protected:
  String name;
  int pin;
  int mode;
  bool value;
  bool changed;
};

class AnalogInput : public GPIOPort {
public:
  AnalogInput(const char *name, int _pin) : GPIOPort(name, _pin) {}
  void setMode(int _mode) {}
  void setValue(bool _value) {}
  int getValue() {
    return analogRead(pin);
  };
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
