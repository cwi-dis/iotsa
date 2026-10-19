#include "iotsaLightMod.h"

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
  IotsaSerial.print("light ");
  IotsaSerial.print(light);
  IotsaSerial.print(" min ");
  IotsaSerial.print(minLight);
  IotsaSerial.print(" max ");
  IotsaSerial.print(maxLight);
  IotsaSerial.print(" level ");
  IotsaSerial.println(lightLevel);
}

// Implementation of the Light module
void IotsaLightMod::setup() {
  // Nothing to do during early initialization for this module
}

bool IotsaLightMod::getHandler(const char *path, JsonObject& reply) {
  reply["light"] = light;
  reply["minLight"] = minLight;
  reply["maxLight"] = maxLight;
  reply["lightLevel"] = lightLevel;
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaLightMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx.status("Light level", "lightLevel", String((int)(lightLevel*100)) + "%");
  ctx.status("Raw value", "light", String(light) + " (seen " + String(minLight) + " to " + String(maxLight) + ")");
}
#endif

void IotsaLightMod::lateSetup() {
  // A GET-only API, which also gives the web page.
  api.setup("light", true);
  name = "light";
}

String IotsaLightMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>Lightlevel is ";
  rv += String((int)(lightLevel*100));
  rv += "%. See <a href=\"/light\">/light</a>, or <a href=\"/api/light\">/api/light</a> for JSON data.</p>";
  return rv;
}

void IotsaLightMod::loop() {
  _update();
}
