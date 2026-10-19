#ifndef _IOTSALIGHTMOD_H_
#define _IOTSALIGHTMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

#define DECAY 200 // The bigger this number the slower the decay. 200 is a few seconds.

//
// Light module. Gets light data from an LDR connected to an analog input. Normalizes it.
// Read-only: a web page and GET /api/light.
//
class IotsaLightMod : public IotsaModule {
public:
  IotsaLightMod(IotsaApplication &_app)
  : IotsaModule(_app),
  light(0),
  minLight(0xffff),
  maxLight(0)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Light"; }
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  void _update();
  unsigned int light;
  unsigned int minLight;
  unsigned int maxLight;
  float lightLevel;
};

#endif
