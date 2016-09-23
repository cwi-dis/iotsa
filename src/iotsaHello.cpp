#include <ESP.h>
#include "iotsaHello.h"

String greeting;

void IotsaHelloMod::setup() {
}

void
IotsaHelloMod::handler() {
  LED digitalWrite(led, 1);
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "greeting") {
      greeting = server.arg(i);
    }
  }
  String message = "<html><head><title>Hello Server</title></head><body><h1>Hello Server</h1>";
  message += "<form method='get'>Greeting: <input name='greeting' value='";
  message += greeting;
  message += "'></form></body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void IotsaHelloMod::serverSetup() {
  server.on("/hello", std::bind(&IotsaHelloMod::handler, this));
}

String IotsaHelloMod::info() {
  return "<p>See <a href=\"/hello\">/hello</a> for info.</p>";
}

void IotsaHelloMod::loop() {
  
}
