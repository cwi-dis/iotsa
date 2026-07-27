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

void
IotsaLightMod::handler() {
  // Handles the page that is specific to the Light module, greets the user and
  // optionally stores a new name to greet the next time.
  String message = "{\"light\":";
  message += String(light);
  message += ",\"minLight\":";
  message += String(minLight);
  message += ",\"maxLight\":";
  message += String(maxLight);
  message += ",\"lightLevel\":";
  message += String(lightLevel);
  message += "}\n";
  server->send(200, "application/json", message);
}

void IotsaLightMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/light", std::bind(&IotsaLightMod::handler, this));
}

String IotsaLightMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>Lightlevel is ";
  rv += String((int)(lightLevel*100));
  rv += "%. See <a href=\"/light\">/light</a> for JSON data.</p>";
  return rv;
}

void IotsaLightMod::loop() {
  _update();
}
