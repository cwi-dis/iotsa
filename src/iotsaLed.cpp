#include "iotsaLed.h"
#include "iotsaRunmode.h"   // IotsaRunmodeMod::instance()/addIdentifyCallback() (cwi-dis/iotsa#176)

// Single-NeoPixel renderer: an explicit set() pattern (finite on/off/count
// blink) takes priority while it's running; otherwise this polls
// iotsaStatus.statusColor() every loop() call (cwi-dis/iotsa#176 -- inverts
// the old push model, see IotsaStatus's own docs).

IotsaLedMod::IotsaLedMod(IotsaApplication &_app, int pin, neoPixelType t, IotsaAuthMod *_auth)
:	IotsaModule(_app, _auth, true),
	strip(1, pin, t),
	rgb(0),
	nextChangeTime(0),
	remainingCount(0),
	onDuration(0),
	offDuration(0),
	isOn(false),
	lastShownColor(0xffffffff)  // deliberately not a valid 0xRRGGBB tint, forces the first poll to render
{
}

void IotsaLedMod::setup() {
  strip.begin();
  strip.show();
  // Default identify() handler (cwi-dis/iotsa#176/#133): two full-intensity
  // flashes then resume. IotsaRunmodeMod is core-tier, ensure()d before any
  // module's setup() runs (iotsa.cpp), so instance() is never null here.
  IotsaRunmodeMod *runmode = IotsaRunmodeMod::instance();
  if (runmode) {
    runmode->addIdentifyCallback([this]() { set(0xffffff, 300, 300, 2); });
  }
}

void IotsaLedMod::loop() {
  if (nextChangeTime != 0) {
    // A set() pattern is in flight -- finite on/off/count blink, unchanged.
    if (millis() < nextChangeTime) return;
    if (isOn) {
      strip.setPixelColor(0, 0);
      strip.show();
      isOn = false;
      lastShownColor = 0;
      if (remainingCount <= 0) {
        nextChangeTime = 0;  // pattern done -- next loop() call resumes status polling
        return;
      }
      nextChangeTime = millis() + offDuration;
    } else {
      strip.setPixelColor(0, rgb);
      strip.show();
      isOn = true;
      lastShownColor = rgb;
      nextChangeTime = millis() + onDuration;
      remainingCount--;
    }
    return;
  }
  // Idle: poll status continuously. Breathe needs ~20-30fps to read smoothly;
  // a single-pixel strip.show() is ~30us, so polling every loop() call is free
  // (design comment, section 5). Only push to the strip when the colour changes.
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

void IotsaLedMod::set(uint32_t _rgb, int _onDuration, int _offDuration, int _count) {
  rgb = _rgb;
  onDuration = _onDuration;
  offDuration = _offDuration;
  remainingCount = _count;
  isOn = false;
  nextChangeTime = millis();
}

void IotsaLedMod::showStatus() {
  nextChangeTime = 0;
  isOn = false;
  loop();  // update immediately rather than waiting for the next loop() call
}
