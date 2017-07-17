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
// This example is used for testing the logger.

#include <ESP.h>
#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaSimple.h"
#include "iotsaLogger.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

ESP8266WebServer server(80);
IotsaApplication application(server, "Iotsa Logging Hello World Server");
IotsaWifiMod wifiMod(application);
IotsaLoggerMod loggerMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

//
// Hello "name" module. Greets visitors to the /hello page, and allows
// them to change the name by which they are greeted.
//

String greeting;

String helloInfo() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/hello\">/hello</a> for info, ";
  if (greeting == "") {
  	rv += "and to set the name to be greeted by.";
  } else {
  	rv += "or to change the name \"";
  	rv += IotsaMod::htmlEncode(greeting);
  	rv += "\" that is currently greeted.";
  }
  rv += "</p>";
  IotsaSerial.print("Logging Hello: info called, greeting=");
  IotsaSerial.println(greeting);
  return rv;
}

void
helloHandler() {
  // Handles the page that is specific to the Hello module, greets the user and
  // optionally stores a new name to greet the next time.
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "greeting") {
      greeting = server.arg(i);
    }
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<p>Hello, " + IotsaMod::htmlEncode(greeting) + "!</p>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += IotsaMod::htmlEncode(greeting);
  message += "'></form></body></html>";
  server.send(200, "text/html", message);
  IotsaSerial.print("Logging Hello: handler called, greeting=");
  IotsaSerial.println(greeting);
}

// Instantiate the Hello module, and install it in the framework
IotsaSimpleMod helloMod(application, "/hello", helloHandler, helloInfo);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  IotsaSerial.println("Logging Hello: setup called");
  application.serverSetup();
  // Add your setup code here.
  ESP.wdtEnable(WDTO_120MS);
  IotsaSerial.println("Logging Hello: setup returning");
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
  // Add your loop code here.
}
