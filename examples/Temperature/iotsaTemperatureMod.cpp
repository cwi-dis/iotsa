#include "iotsaTemperatureMod.h"

// Implementation of the Temperature module. Like Light (read that one first)
// it only measures, so it has a getHandler() but no putHandler(), and a web
// page without a form.
//
// The sensor is handled by a library, DHT (see mainTemperature.cpp for how to
// install it). The module only calls it: dht.begin() in setup(), and
// dht.readTemperature()/readHumidity() in _update(). This is how you would use
// most sensor libraries in iotsa.
//
// Unlike Light it doesn't measure in loop(), but only when someone asks: on
// the web page, GET /api/temperature, or the main page. A DHT sensor is slow
// and may only be read every 2 seconds; the DHT library takes care of that,
// and gives the previous values when asked sooner.

void IotsaTemperatureMod::setup() {
  dht.begin();
}

void IotsaTemperatureMod::lateSetup() {
  // api.setup("temperature", true) gives GET on /api/temperature and the web
  // page /temperature. No PUT: there is nothing to set.
  api.setup("temperature", true);
  name = "temperature";
}

void IotsaTemperatureMod::loop() {
  // Nothing to do: we measure when asked, see _update().
}

// No configLoad()/configSave(): nothing is saved.

// Read the sensor. If it doesn't answer (not connected, wrong pin or type)
// the values are NAN, "not a number".
void IotsaTemperatureMod::_update() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

// No putHandler(): lateSetup() asked for no PUT, so it would never be called.

// A value that is NAN is sent as null.
bool IotsaTemperatureMod::getHandler(const char *path, JsonObject& reply) {
  _update();
  reply["temperature"] = temperature;
  reply["humidity"] = humidity;
  return true;
}

#ifdef IOTSA_WITH_WEB
// No formHandler_args(): the page has no form, so there is nothing to pick up.

// The page only shows the values, so there is only an everyday part.
void IotsaTemperatureMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  _update();
  if (isnan(temperature)) {
    ctx.status("Temperature", "temperature", "no reading, check the sensor");
    return;
  }
  ctx.status("Temperature", "temperature", String(temperature) + " °C");
  ctx.status("Humidity", "humidity", String(humidity) + "%");
}
#endif // IOTSA_WITH_WEB

String IotsaTemperatureMod::info() {
  _update();
  String rv = "<p>Temperature is ";
  rv += String(temperature);
  rv += " °C, humidity is ";
  rv += String(humidity);
  rv += "%. See <a href=\"/temperature\">/temperature</a>, or <a href=\"/api/temperature\">/api/temperature</a> for JSON data.</p>";
  return rv;
}
