#ifndef _IOTSATEMPERATUREMOD_H_
#define _IOTSATEMPERATUREMOD_H_

#include "iotsa.h"
#include <DHT.h>

//
// Temperature module. Gets temperature and humidity data from a DHT21 module.
//
class IotsaTemperatureMod : public IotsaBaseModule {
public:
  IotsaTemperatureMod(IotsaApplication &_app, int pin, int type)
  : IotsaBaseModule(_app),
    dht(pin, type)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
private:
  void handler();
  void _update();
  float temperature;
  float humidity;
  DHT dht;
};

#endif
