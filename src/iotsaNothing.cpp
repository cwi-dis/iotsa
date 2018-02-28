#include "iotsaNothing.h"
#include "iotsaConfigFile.h"
void
IotsaNothingMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "argument") {
    	if (needsAuthentication()) return;
    	argument = server.arg(i);
    	anyChanged = true;
    }
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Boilerplate module</title></head><body><h1>Boilerplate module</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  message += htmlEncode(argument);
  message += "'><br><input type='submit'></form>";
  server.send(200, "text/html", message);
}

void IotsaNothingMod::setup() {
  configLoad();
}

bool IotsaNothingMod::getHandler(const char *path, JsonObject& reply) {
  reply["argument"] = argument;
  return true;
}

bool IotsaNothingMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("argument")) {
    argument = reqObj.get<String>("argument");
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}

void IotsaNothingMod::serverSetup() {
  server.on("/nothing", std::bind(&IotsaNothingMod::handler, this));
  api.setup("/api/nothing", true, true);
}

void IotsaNothingMod::configLoad() {
  IotsaConfigFileLoad cf("/config/nothing.cfg");
  cf.get("argument", argument, "");
 
}

void IotsaNothingMod::configSave() {
  IotsaConfigFileSave cf("/config/nothing.cfg");
  cf.put("argument", argument);
}

void IotsaNothingMod::loop() {
}

String IotsaNothingMod::info() {
  String message = "<p>Built with boilerplate module. See <a href=\"/nothing\">/nothing</a> to change the boilerplate module argument.</p>";
  return message;
}
