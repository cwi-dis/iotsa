#include "iotsaDimmerMod.h"

// Implementation of the Dimmer module. It is built like the Light module
// (read that one first): read-only, a getHandler() and a page without a form.
// What's new is where the values come from: the button and the encoder
// change them directly, through bindVar().

void IotsaDimmerMod::setup() {
  // Every press of the button toggles on (true: toggle; false would make on
  // follow the button, true while it is held down).
  button.bindVar(on, true);
  // Every step of the encoder changes level by 5, between 0 and 100.
  encoder.bindVar(level, 0, 100, 5);
  // A callback runs after every change. Here it only logs; a real dimmer
  // would set its lamp.
  button.setCallback([this]() {
    IotsaSerial.printf("dimmer: on=%d\n", on);
    return true;
  });
  encoder.setCallback([this]() {
    IotsaSerial.printf("dimmer: level=%d\n", level);
    return true;
  });
}

void IotsaDimmerMod::lateSetup() {
  // GET only: the values are set with the controls, not over the network.
  api.setup("dimmer", true);
  name = "dimmer";
}

// loop() is empty (in the .h): the input module reads the controls in its
// own loop(), and they change on and level for us.

// No configLoad()/configSave(): after a reboot the dimmer is off, at 50.

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
