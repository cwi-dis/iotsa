#ifndef _IOTSALED_H_
#define _IOTSALED_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Adafruit_NeoPixel.h>

class IotsaLedMod : public IotsaModule {
public:
  IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t=NEO_GRB + NEO_KHZ800, IotsaAuthMod *_auth=NULL);
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  Adafruit_NeoPixel strip;
  uint32_t lastShownColor;  // dedup: only strip.show() when the polled colour changes
};

#endif
