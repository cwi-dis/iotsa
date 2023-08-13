//
// Another version of Hello. This version uses the C++ interface (like HelloCpp)
// and in addition provides a REST interface to change the name.
//

#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaWifi.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Hello World Server with API");

#include "iotsaStandardModules.h"

//
// Hello "name" module. Greets visitors to the /hello page, and allows
// them to change the name by which they are greeted.
//

// Declaration of the Hello module
class IotsaHelloMod : public IotsaApiMod {
public:
  IotsaHelloMod(IotsaApplication &_app) : IotsaApiMod(_app) {}
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
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
  if( server->hasArg("greeting")) {
    greeting = server->arg("greeting");
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += htmlEncode(greeting);
  message += "'></form></body></html>";
  server->send(200, "text/html", message);
}

bool IotsaHelloMod::getHandler(const char *path, JsonObject& reply) {
  reply["greeting"] = greeting;
  return true;
}

bool IotsaHelloMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonVariant arg = request["greeting"];
  if (arg.is<const char*>()) {
    greeting = arg.as<String>();
    return true;
  }
  return false;
}

void IotsaHelloMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/hello", std::bind(&IotsaHelloMod::handler, this));
  api.setup("/api/hello", true, true);
  name = "hello";
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
}
 
// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}

