#include "iotsaSimpleIO.h"
#include "iotsaConfigFile.h"
void
IotsaSimpleIOMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "argument") {
    	if (needsAuthentication()) return;
    	argument = server.arg(i);
    	anyChanged = true;
    }
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Simple GPIO module</title></head><body><h1>Simple GPIO</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  message += argument;
  message += "'><br><input type='submit'></form>";
  server.send(200, "text/html", message);
}

void IotsaSimpleIOMod::setup() {
  configLoad();
}

void IotsaSimpleIOMod::serverSetup() {
  server.on("/io", std::bind(&IotsaSimpleIOMod::handler, this));
}

void IotsaSimpleIOMod::configLoad() {
  IotsaConfigFileLoad cf("/config/SimpleIO.cfg");
  cf.get("argument", argument, "");
 
}

void IotsaSimpleIOMod::configSave() {
  IotsaConfigFileSave cf("/config/SimpleIO.cfg");
  cf.put("argument", argument);
}

void IotsaSimpleIOMod::loop() {
}

String IotsaSimpleIOMod::info() {
  String message = "<p>Built with Simple IO module. See <a href=\"/io\">/io</a> to examine GPIO pins and/or change them.</p>";
  return message;
}
