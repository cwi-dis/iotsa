#include "iotsaTemperatureMod.h"

void IotsaTemperatureMod::_update() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

// Implementation of the Temperature module
void IotsaTemperatureMod::setup() {
  // Nothing to do during early initialization for this module
}

bool IotsaTemperatureMod::getHandler(const char *path, JsonObject& reply) {
  _update();
  reply["temperature"] = temperature;
  reply["humidity"] = humidity;
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaTemperatureMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  _update();
  ctx.status("Temperature", "temperature", String(temperature) + " °C");
  ctx.status("Humidity", "humidity", String(humidity) + "%");
}
#endif

void IotsaTemperatureMod::lateSetup() {
  // A GET-only API, which also gives the web page.
  api.setup("temperature", true);
  name = "temperature";
}

String IotsaTemperatureMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>Temperature is ";
  rv += String(temperature);
  rv += ", humidity is ";
  rv += String(humidity);
  rv += ". See <a href=\"/temperature\">/temperature</a>, or <a href=\"/api/temperature\">/api/temperature</a> for JSON data.</p>";
  return rv;
}

void IotsaTemperatureMod::loop() {
  // Nothing to do in the loop, for this module
}
