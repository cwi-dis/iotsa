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
// This version requires a username/password to change the greeting, to enable
// over-the-air updating and to change the WiFi configuration.
// The usernames/password combination is changeable.
//

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaUser.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

//
// Hello "name" module. Greets visitors to the /hello page, and allows
// them to change the name by which they are greeted.
//

// Declaration of the Hello module
class IotsaHelloMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
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
      if (needsAuthentication()) {
        return;
      }
      greeting = server.arg(i);
    }
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += greeting;
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
  	rv += greeting;
  	rv += " that is currently greeted.";
  }
  rv += "</p>";
  return rv;
}

void IotsaHelloMod::loop() {
  // Nothing to do in the loop, for this module
}

//
// Instantiate all the objects we need.
//
ESP8266WebServer server(80);  // The web server
IotsaApplication application(server, "Iotsa Hello World Server"); // The application framework

//
// Authentication class. Pass in default username, default password is
// set base on ESP8266 identity.
//
IotsaUserMod myAuthenticator(application, "owner");  // Our authenticator module

IotsaWifiMod wifiMod(application, &myAuthenticator);  // The network configuration module (authenticated)

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, &myAuthenticator);  // The over-the-air updater module (authenticated)
#endif

IotsaHelloMod helloMod(application, &myAuthenticator); // Our hello module (authenticated)

// Standard setup() method, hands off everything to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
}
 
// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}

