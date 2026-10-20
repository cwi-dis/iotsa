#ifndef _IOTSALIGHTMOD_H_
#define _IOTSALIGHTMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

#define SAMPLE_INTERVAL 20 // Milliseconds between measurements.
#define DECAY 200 // The bigger this number the slower the level follows the light. 200 is a few seconds.

//
// Light module. Gets light data from an LDR connected to an analog input. Normalizes it.
// Read-only: a web page and GET /api/light. Nothing is saved: after a reboot it
// learns the darkest and brightest values again.
//
class IotsaLightMod : public IotsaModule {
public:
  IotsaLightMod(IotsaApplication &_app)
  : IotsaModule(_app),
  light(0),
  minLight(0xffff),
  maxLight(0),
  lightLevel(0.5),
  lastSample(0)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  void _update();
  bool getHandler(const char *path, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Light"; }
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
private:
  unsigned int light;
  unsigned int minLight;
  unsigned int maxLight;
  float lightLevel;
  unsigned long lastSample;
};

#endif
