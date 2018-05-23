//
// This example is used for showing the logger. It is the same as the "hello" example,
// but also allows access to the log (which keeps limited output that has been sent to
// the Serial serial port)
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaSimple.h"
#include "iotsaLogger.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Logging Hello World Server");
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
  // Return some information about this module, for the main page of the web server->
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
  if( server->hasArg("greeting")) {
    greeting = server->arg("greeting");
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<p>Hello, " + IotsaMod::htmlEncode(greeting) + "!</p>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += IotsaMod::htmlEncode(greeting);
  message += "'></form></body></html>";
  server->send(200, "text/html", message);
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
  IotsaSerial.println("Logging Hello: setup returning");
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
  // Add your loop code here.
}
