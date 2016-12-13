#ifndef _IOTSALED_H_
#define _IOTSALED_H_
#include "iotsa.h"
#include <Adafruit_NeoPixel.h>

class IotsaLedMod : public IotsaMod {
public:
  IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t=NEO_GRB + NEO_KHZ800);
  void setup();
  void serverSetup();
  void loop();
  String info();
  void set(uint32_t _rgb, int _onDuration, int _offDuration, int _count);
protected:
  Adafruit_NeoPixel strip;
  uint32_t rgb;
  uint32_t nextChangeTime;
  int remainingCount;
  int onDuration;
  int offDuration;
  bool isOn;
};

#endif
