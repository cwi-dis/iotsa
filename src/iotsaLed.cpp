#include "iotsaLed.h"
#include "iotsaRunmode.h"   // IotsaRunmodeMod::instance()/addIdentifyCallback() (cwi-dis/iotsa#176)

// Single-NeoPixel renderer: polls iotsaStatus.statusColor() every loop() call
// (cwi-dis/iotsa#176 -- inverts the old push model, see IotsaStatus's own docs).
// Any caller that wants to override the pixel temporarily -- identify(), or an
// external party via IotsaLedControlMod (examples/Led) -- goes through
// iotsaStatus.setStatusPulse(), which statusColor() already gives top
// precedence, rather than a side-channel here (cwi-dis/iotsa#256).

IotsaLedMod::IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t, IotsaAuthMod *_auth)
:	IotsaModule(_app, _auth, true),
	strip(1, pin, t),
	lastShownColor(0xffffffff)  // deliberately not a valid 0xRRGGBB tint, forces the first poll to render
{
}

void IotsaLedMod::setup() {
  strip.begin();
  strip.show();
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
    strip.setPixelColor(0, colour);
    strip.show();
    lastShownColor = colour;
  }
}

void IotsaLedMod::lateSetup() {
	name = "led";
}

#ifdef IOTSA_WITH_WEB
String IotsaLedMod::info() {
	return "";
}
#endif
