#include "iotsaHelloMod.h"

// Implementation of the Hello module
void IotsaHelloMod::setup() {
  // Nothing to do during early initialization for this module
}

void
IotsaHelloMod::webHandler() {
  // Handles the page that is specific to the Hello module, greets the user and
  // optionally stores a new name to greet the next time.
  if (server->hasArg("greeting")) {
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
  api.setup("hello", true, true);
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
