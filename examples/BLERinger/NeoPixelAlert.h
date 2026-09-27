#ifndef _NEOPIXELALERT_H_
#define _NEOPIXELALERT_H_
#include "iotsa.h"
#include <NeoPixelBus.h>
#include "iotsaImmediateAlertBLE.h"

#ifndef IOTSA_PIN_NEOPIXEL
#define IOTSA_PIN_NEOPIXEL 8 // esp32c3devkit's onboard addressable RGB LED
#endif

// Drives the board's single onboard NeoPixel to visualize an Immediate
// Alert Service alert level (off / dim blue / bright red for
// none/mild/high), auto-off after a fixed duration -- same timeout-based
// auto-off shape as examples/Ringer's IotsaAlarmMod, just an LED instead of
// a buzzer pin.
class NeoPixelAlert {
public:
  void setup();
  void loop();
  void setAlertLevel(uint8_t level);
private:
  typedef NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> Strip;
  Strip *strip = nullptr;
  uint32_t alertEndAtMillis = 0;
  static const uint32_t alertDurationMillis = 3000;
};
#endif
