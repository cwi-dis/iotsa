#ifndef _IOTSASIMPLEIO_H_
#define _IOTSASIMPLEIO_H_
#include "iotsa.h"

#define PWM_OUTPUT OUTPUT + 42

class GPIOPort {
public:
  GPIOPort(const char* _name, int _pin)
  : name(_name),
    pin(_pin),
    mode(INPUT),
    value(false),
    changed(false)
  {}
  virtual bool setMode(int _mode) = 0;
  virtual void setValue(int _value) = 0;
  virtual int getValue() = 0;
  int getMode() {
    return mode;
  }
  String name;

protected:
  int pin;
  int mode;
  int value;
  bool changed;
};

class DigitalPort : public GPIOPort {
public:
  DigitalPort(const char* _name, int _pin) : GPIOPort(_name, _pin) {}
  virtual bool setMode(int _mode) {
    if (mode == _mode) return false;
    mode = _mode;
    if (mode == PWM_OUTPUT) mode = OUTPUT;
    pinMode(pin, mode);
    return true;
  };
  virtual void setValue(int _value) {
    value = _value;
    if (mode == OUTPUT)
      digitalWrite(pin, value ? HIGH : LOW);
     else if (mode == PWM_OUTPUT)
      analogWrite(pin, value);
  };
  virtual int getValue() {
    if (mode == OUTPUT) return value;
    return digitalRead(pin);
  };
};

class AnalogInput : public GPIOPort {
public:
  AnalogInput(const char *name, int _pin) : GPIOPort(name, _pin) {}
  virtual bool setMode(int _mode) { return false; }
  virtual void setValue(int _value) {}
  virtual int getValue() {
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
  void apiHandler();
  String argument;
};

#endif
