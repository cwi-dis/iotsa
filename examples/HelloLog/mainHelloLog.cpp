//
// The Hello example (read that one first) with a log: the device keeps the
// last part of what it printed to the serial port, and shows it on the web
// page /logger. Handy for a device that isn't connected to a computer.
//
// Everything that is new compared to Hello is marked "Log:".
//

#include "iotsa.h"
#include "iotsaSimple.h"
#include "iotsaLogger.h"

// As in Hello: because we want to greet people by name, we need a variable
// holding that name, which they can change on the web page.
String personName = "human";

// In iotsa, you need to instantiate an application and a module (to handle
// the web page, created at the end of this file). The iotsa framework will
// then handle WiFi, over-the-air programming and other goodies for you.
IotsaApplication application("Iotsa Logging Hello World Server");

// Log: the logger module. From now on, everything printed with IotsaSerial
// (not Serial) goes to the serial port and also into the log. The log holds
// the last 4096 bytes (IOTSA_LOGGER_BUFFER_SIZE).
IotsaLoggerMod loggerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
  // Add your setup code here.
  // Log: something to find in the log.
  IotsaSerial.println("Logging Hello: setup done");
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
  // Add your loop code here.
}

// Now we get iotsa-specific code.
// Handles the page that is specific to the Hello module, greets the user and
// optionally stores a new name to greet the next time.
void
helloHandler() {
  if( application.server->hasArg("personName")) {
    personName = application.server->arg("personName");
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<p>Hello, " + IotsaBaseModule::htmlEncode(personName) + "!</p>";
  message += "<form method='get'>Name: <input name='personName' value='";
  message += IotsaBaseModule::htmlEncode(personName);
  message += "'></form></body></html>";
  application.server->send(200, "text/html", message);
  // Log: something to find in the log.
  IotsaSerial.print("Logging Hello: page shown, personName=");
  IotsaSerial.println(personName);
}

// Return some information about this module, for the main page of the web server.
String helloInfo() {
  String rv = "<p>See <a href=\"/hello\">/hello</a> to change the name \"";
  rv += IotsaBaseModule::htmlEncode(personName);
  rv += "\" that is currently greeted.";
  rv += "</p>";
  return rv;
}

// Finally we need to create an iotsa module with the two functions written
// above and add it to the iotsa application.
IotsaSimpleMod helloMod(application, "/hello", helloHandler, helloInfo);
