#include "iotsaLed.h"
#include "iotsaRunmode.h"   // IotsaRunmodeMod::instance()/addIdentifyCallback() (cwi-dis/iotsa#176)

// Status LED renderer (NeoPixel or plain LED): polls iotsaStatus.statusColor() every loop() call
// (cwi-dis/iotsa#176 -- inverts the old push model, see IotsaStatus's own docs).
// Any caller that wants to override the pixel temporarily -- identify(), or an
// external party via IotsaLedControlMod (examples/Led) -- goes through
// iotsaStatus.setStatusPulse(), which statusColor() already gives top
// precedence, rather than a side-channel here (cwi-dis/iotsa#256).

IotsaLedMod::IotsaLedMod(IotsaApplication &_app, int _pin, neoPixelType t)
:	IotsaModule(_app, true),
	pin(_pin),
	strip(new Adafruit_NeoPixel(1, _pin, t)),
	lastShownColor(0xffffffff)  // deliberately not a valid 0xRRGGBB tint, forces the first poll to render
{
  claimSingleton(this);
}

#ifdef IOTSA_PIN_LED
IotsaLedMod::IotsaLedMod(IotsaApplication &_app)
:	IotsaModule(_app, true),
	pin(IOTSA_PIN_LED),
	lastShownColor(0xffffffff)
{
#ifdef IOTSA_LED_NEOPIXEL
  strip = new Adafruit_NeoPixel(1, IOTSA_PIN_LED, NEO_GRB + NEO_KHZ800);
#endif
#ifdef IOTSA_LED_ACTIVE_LOW
  activeLow = true;
#endif
  claimSingleton(this);
}
#endif

void IotsaLedMod::setup() {
  if (strip) {
    strip->begin();
    strip->show();
  } else {
    pinMode(pin, OUTPUT);
    _show(0);
  }
  // Default identify() handler (cwi-dis/iotsa#176/#133): two full-intensity
  // flashes, via the pulse channel like any other transient signal. 300+300
  // twice = 1200ms. IotsaRunmodeMod is core-tier, ensure()d before any
  // module's setup() runs (iotsa.cpp), so instance() is never null here.
  IotsaRunmodeMod *runmode = IotsaRunmodeMod::instance();
  if (runmode) {
    runmode->addIdentifyCallback([]() { iotsaStatus.setStatusPulse(0xffffff, 300, 300, 1200, "identify"); });
  }
}

void IotsaLedMod::loop() {
  // Breathe needs ~20-30fps to read smoothly; a single-pixel strip.show() is
  // ~30us, so polling every loop() call is free (design comment, section 5).
  // Only push to the strip when the colour changes.
  uint32_t colour = iotsaStatus.statusColor();
  if (colour != lastShownColor) {
    _show(colour);
    lastShownColor = colour;
  }
}

void IotsaLedMod::_show(uint32_t colour) {
  if (strip) {
    strip->setPixelColor(0, colour);
    strip->show();
    return;
  }
  // Plain LED: the brightest channel becomes the brightness. Status colours use
  // the dim 0x3f level, scaled up here to (nearly) full; pulses like identify
  // use 0xff and are capped.
  uint32_t level = max(max((colour >> 16) & 0xff, (colour >> 8) & 0xff), colour & 0xff);
  uint32_t brightness = level * 4;
  if (brightness > 255) brightness = 255;
  analogWrite(pin, activeLow ? 255 - brightness : brightness);
}

void IotsaLedMod::lateSetup() {
	name = "led";
}

#ifdef IOTSA_WITH_WEB
String IotsaLedMod::info() {
	return "";
}
#endif
