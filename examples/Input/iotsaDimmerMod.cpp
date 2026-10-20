#include "iotsaDimmerMod.h"

// Implementation of the Dimmer module. It is built like the Light module
// (read that one first): read-only, a getHandler() and a page without a form.
// What's new is where the values come from: the button and the encoder
// change them directly, through bindVar(). And it drives an LED, with PWM.

void IotsaDimmerMod::setup() {
  // Every press of the button toggles on (true: toggle; false would make on
  // follow the button, true while it is held down).
  button.bindVar(on, true);
  // Every step of the encoder changes level by 5, between 0 and 100.
  encoder.bindVar(level, 0, 100, 5);
  // A callback runs after every change: set the LED.
  button.setCallback([this]() { _update(); return true; });
  encoder.setCallback([this]() { _update(); return true; });
  pinMode(pin, OUTPUT);
  _update();
}

void IotsaDimmerMod::lateSetup() {
  // GET only: the values are set with the controls, not over the network.
  api.setup("dimmer", true);
  name = "dimmer";
}

// loop() is empty (in the .h): the input module reads the controls in its
// own loop(), and they change on and level for us.

// No configLoad()/configSave(): after a reboot the dimmer is off, at 50.

// Set the LED's brightness. analogWrite() switches the pin on and off very
// fast (PWM); 0 is always off, 255 always on.
void IotsaDimmerMod::_update() {
  int brightness = on ? level * 255 / 100 : 0;
  analogWrite(pin, activeLow ? 255 - brightness : brightness);
}

// No putHandler(): lateSetup() asked for no PUT.

bool IotsaDimmerMod::getHandler(const char *path, JsonObject& reply) {
  reply["on"] = on;
  reply["level"] = level;
  return true;
}

#ifdef IOTSA_WITH_WEB
// No formHandler_args(): the page has no form.

void IotsaDimmerMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx.status("On", "on", on ? "yes" : "no");
  ctx.status("Level", "level", String(level));
}
#endif // IOTSA_WITH_WEB

String IotsaDimmerMod::info() {
  return "<p>A dimmer, set with a button and a rotary encoder. See <a href='/dimmer'>/dimmer</a>, or <a href='/api/dimmer'>/api/dimmer</a> for JSON data.</p>";
}
