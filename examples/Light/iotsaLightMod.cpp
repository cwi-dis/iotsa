#include "iotsaLightMod.h"

// Implementation of the Light module. It is built like the Hello module in
// HelloIotsa (read that one first), but it only measures: there is nothing
// for the outside world to set. So it has a getHandler() but no putHandler(),
// and a web page without a form.

void IotsaLightMod::setup() {
  // Nothing to do: analogRead() needs no preparation.
}

void IotsaLightMod::lateSetup() {
  // api.setup("light", true) gives GET on /api/light and the web page /light.
  // No PUT: there is nothing to set.
  api.setup("light", true);
  name = "light";
}

// Measure every SAMPLE_INTERVAL milliseconds. The loop runs much more often
// than that, so most of the time there is nothing to do.
void IotsaLightMod::loop() {
  if (millis() - lastSample < SAMPLE_INTERVAL) return;
  lastSample = millis();
  _update();
}

// No configLoad()/configSave(): nothing is saved. After a reboot the module
// learns the darkest and brightest values again.

// Read the LDR and turn it into a level between 0 and 1: 0 is the darkest it
// has seen since the last reboot, 1 the brightest. The level moves slowly
// towards each new reading, so a short flicker hardly changes it.
void IotsaLightMod::_update() {
  light = analogRead(A0);
  if (light < minLight) minLight = light;
  if (light > maxLight) maxLight = light;
  if (minLight == maxLight) {
    lightLevel = 0.5;
  } else {
    float newLightLevel = (float)(light-minLight)/(maxLight-minLight);
    lightLevel = ((DECAY-1)*lightLevel+newLightLevel)/DECAY;
  }
}

// No putHandler(): lateSetup() asked for no PUT, so it would never be called.

bool IotsaLightMod::getHandler(const char *path, JsonObject& reply) {
  reply["light"] = light;
  reply["minLight"] = minLight;
  reply["maxLight"] = maxLight;
  reply["lightLevel"] = lightLevel;
  return true;
}

#ifdef IOTSA_WITH_WEB
// No formHandler_args(): the page has no form, so there is nothing to pick up.

// The page only shows the values. Nothing is configured, so there is only an
// everyday part.
void IotsaLightMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx.status("Light level", "lightLevel", String((int)(lightLevel*100)) + "%");
  ctx.status("Raw value", "light", String(light) + " (seen " + String(minLight) + " to " + String(maxLight) + ")");
}
#endif // IOTSA_WITH_WEB

String IotsaLightMod::info() {
  String rv = "<p>Lightlevel is ";
  rv += String((int)(lightLevel*100));
  rv += "%. See <a href=\"/light\">/light</a>, or <a href=\"/api/light\">/api/light</a> for JSON data.</p>";
  return rv;
}
