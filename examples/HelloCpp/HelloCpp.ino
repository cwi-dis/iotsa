//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
// A "hello" module is added, which greets the user with a name settable through
// a web form (not kept over reboots). 
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaWebServer server(80);
IotsaApplication application(server, "Iotsa Hello World Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// Hello "name" module. Greets visitors to the /hello page, and allows
// them to change the name by which they are greeted.
//

// Declaration of the Hello module
class IotsaHelloMod : public IotsaMod {
public:
  IotsaHelloMod(IotsaApplication &_app) : IotsaMod(_app) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void handler();
};

String greeting;

// Implementation of the Hello module
void IotsaHelloMod::setup() {
	// Nothing to do during early initialization for this module
}

void
IotsaHelloMod::handler() {
  // Handles the page that is specific to the Hello module, greets the user and
  // optionally stores a new name to greet the next time.
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "greeting") {
      greeting = server.arg(i);
    }
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += htmlEncode(greeting);
  message += "'></form></body></html>";
  server.send(200, "text/html", message);
}

void IotsaHelloMod::serverSetup() {
  // Setup the web server hooks for this module.
  server.on("/hello", std::bind(&IotsaHelloMod::handler, this));
}

String IotsaHelloMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/hello\">/hello</a> for info, ";
  if (greeting == "") {
  	rv += "and to set the name to be greeted by.";
  } else {
  	rv += "or to change the name ";
  	rv += htmlEncode(greeting);
  	rv += " that is currently greeted.";
  }
  rv += "</p>";
  return rv;
}

void IotsaHelloMod::loop() {
  // Nothing to do in the loop, for this module
}

// Instantiate the Hello module, and install it in the framework
IotsaHelloMod helloMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
#ifndef ESP32
  ESP.wdtEnable(WDTO_120MS);
#endif
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

