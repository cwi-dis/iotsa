#ifndef _IOTSALED_H_
#define _IOTSALED_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include <Adafruit_NeoPixel.h>

// Status LED (cwi-dis/iotsa#176, #272): shows iotsaStatus.statusColor(), in
// colour on a NeoPixel, or as brightness on a plain LED (rhythms and pulses
// survive, the colour is lost).
//
// Created automatically when the board defines IOTSA_PIN_LED, plus
// IOTSA_LED_NEOPIXEL for a NeoPixel or IOTSA_LED_ACTIVE_LOW for a plain LED
// that lights when the pin is low (see iotsa-board.json). Build with
// -DIOTSA_WITHOUT_STATUS_LED for an app that uses that pin for something else.
// A sketch can still declare its own, e.g. for a NeoPixel on another pin; that
// one is then used instead.
class IotsaLedMod : public IotsaModule, public IotsaSingletonModule<IotsaLedMod> {
public:
#ifdef IOTSA_PIN_LED
  // The board's own status LED, as described by its IOTSA_PIN_LED/IOTSA_LED_* defines.
  IotsaLedMod(IotsaApplication &_app);
#endif
  // A NeoPixel on the given pin.
  IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t=NEO_GRB + NEO_KHZ800);
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  int pin;
  bool activeLow = false;
  Adafruit_NeoPixel *strip = nullptr;  // null for a plain LED
  uint32_t lastShownColor;  // dedup: only update the LED when the polled colour changes
  void _show(uint32_t colour);
};

#endif
