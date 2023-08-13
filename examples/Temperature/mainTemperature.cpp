//
// Server that provides a web interface to a DHT21 temperature sensor.
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include <DHT.h>

// CHANGE: Add application includes and declarations here

#define DHT_PIN 13
#define DHT_TYPE DHT21

IotsaApplication application("Iotsa Temperature Server");

#include "iotsaStandardModules.h"

//
// Temperature module. Gets temperature and humidity data from a DHT21 module.
//

// Declaration of the Temperature module
class IotsaTemperatureMod : public IotsaMod {
public:
  IotsaTemperatureMod(IotsaApplication &_app, int pin, int type) 
  : IotsaMod(_app),
    dht(pin, type)
  {}
	void setup() override;
	void serverSetup() override;
	void loop() override;
  String info() override;
private:
  void handler();
  void _update();
  float temperature;
  float humidity;
  DHT dht;
};


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
  server->send(200, "application/json", message);
}

void IotsaTemperatureMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/temperature", std::bind(&IotsaTemperatureMod::handler, this));
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

// Instantiate the Temperature module, and install it in the framework
IotsaTemperatureMod temperatureMod(application, DHT_PIN, DHT_TYPE);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

