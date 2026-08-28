#ifndef _IOTSALIGHTMOD_H_
#define _IOTSALIGHTMOD_H_

#include "iotsa.h"

#define DECAY 200 // The bigger this number the slower the decay. 200 is a few seconds.

//
// Light module. Gets light data from an LDR connected to an analog input. Normalizes it.
//
class IotsaLightMod : public IotsaBaseModule {
public:
  IotsaLightMod(IotsaApplication &_app)
  : IotsaBaseModule(_app),
  light(0),
  minLight(0xffff),
  maxLight(0)
  {}
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
private:
  void handler();
  void _update();
  unsigned int light;
  unsigned int minLight;
  unsigned int maxLight;
  float lightLevel;
};

#endif
