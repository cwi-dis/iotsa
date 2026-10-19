#ifndef _IOTSATEMPERATUREMOD_H_
#define _IOTSATEMPERATUREMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"
#include <DHT.h>

//
// Temperature module. Gets temperature and humidity data from a DHT21 module.
// Read-only: a web page and GET /api/temperature.
//
class IotsaTemperatureMod : public IotsaModule {
public:
  IotsaTemperatureMod(IotsaApplication &_app, int pin, int type)
  : IotsaModule(_app),
    dht(pin, type)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Temperature"; }
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  void _update();
  float temperature;
  float humidity;
  DHT dht;
};

#endif
