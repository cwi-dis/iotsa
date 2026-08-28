#include "iotsaTemperatureMod.h"

void IotsaTemperatureMod::_update() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

// Implementation of the Temperature module
void IotsaTemperatureMod::setup() {
  // Nothing to do during early initialization for this module
}

void
IotsaTemperatureMod::handler() {
  // Handles the page that is specific to the Temperature module, greets the user and
  // optionally stores a new name to greet the next time.
  _update();
  String message = "{\"temperature\":";
  message += String(temperature);
  message += ",\"humidity\":";
  message += String(humidity);
  message += "}\n";
  app.server->send(200, "application/json", message);
}

void IotsaTemperatureMod::lateSetup() {
  // Setup the web server hooks for this module.
  app.server->on("/temperature", std::bind(&IotsaTemperatureMod::handler, this));
}

String IotsaTemperatureMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>Temperature is ";
  rv += String(temperature);
  rv += ", humidity is ";
  rv += String(humidity);
  rv += ". See <a href=\"/temperature\">/temperature</a> for JSON data.</p>";
  return rv;
}

void IotsaTemperatureMod::loop() {
  // Nothing to do in the loop, for this module
}
