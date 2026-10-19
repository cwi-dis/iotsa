//
// Simplest Iotsa service.
//
// A "hello" module is added, which greets the user with a name settable through
// a web form (not kept over reboots).
//
// The structure of this example is very similar to that of a normal Arduino
// program, to show how an existing Arduino program can be given a web interface
// easily with Iotsa.
//

#include "iotsa.h"
#include "iotsaSimple.h"

// Because this is the simplest Hello example, and we want to greet people
// by name, we need a variable holding that name, which they can change on
// the web page.
String greeting;

// In iotsa, you need to instantiate an application and a module (to handle
// the web page, created at the end of this file). The iotsa framework will
// then handle WiFi, over-the-air programming and other goodies for you.
IotsaApplication application("Iotsa Hello World Server");

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
  // Add your setup code here.
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
  if( application.server->hasArg("greeting")) {
    greeting = application.server->arg("greeting");
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<p>Hello, " + IotsaBaseModule::htmlEncode(greeting) + "!</p>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += IotsaBaseModule::htmlEncode(greeting);
  message += "'></form></body></html>";
  application.server->send(200, "text/html", message);
}

// Return some information about this module, for the main page of the web server.
String helloInfo() {
  String rv = "<p>See <a href=\"/hello\">/hello</a> for info, ";
  if (greeting == "") {
  	rv += "and to set the name to be greeted by.";
  } else {
  	rv += "or to change the name \"";
  	rv += IotsaBaseModule::htmlEncode(greeting);
  	rv += "\" that is currently greeted.";
  }
  rv += "</p>";
  return rv;
}

// Finally we need to create an iotsa module with the two functions written
// above and add it to the iotsa application.
IotsaSimpleMod helloMod(application, "/hello", helloHandler, helloInfo);
