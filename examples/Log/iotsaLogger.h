#ifndef _IOTSALOGGER_H_
#define _IOTSALOGGER_H_
#include "iotsa.h"

class IotsaLoggerMod : public IotsaMod {
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

class IotsaLogPrinter : public Print {
public:
  virtual size_t write(uint8_t ch);
};

extern Print *iotsaOverrideSerial;
#define Serial (*iotsaOverrideSerial)
#endif
