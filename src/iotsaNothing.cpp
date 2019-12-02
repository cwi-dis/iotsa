#include "iotsa.h"
#include "iotsaNothing.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_WEB
void
IotsaNothingMod::handler() {
  bool anyChanged = false;
  if( server->hasArg("argument")) {
    if (needsAuthentication()) return;
    argument = server->arg("argument");
    anyChanged = true;
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Boilerplate module</title></head><body><h1>Boilerplate module</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  message += htmlEncode(argument);
  message += "'><br><input type='submit'></form>";
  server->send(200, "text/html", message);
}

String IotsaNothingMod::info() {
  String message = "<p>Built with boilerplate module. See <a href=\"/nothing\">/nothing</a> to change the boilerplate module argument.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaNothingMod::setup() {
  configLoad();
}

#ifdef IOTSA_WITH_API
bool IotsaNothingMod::getHandler(const char *path, JsonObject& reply) {
  reply["argument"] = argument;
  return true;
}

bool IotsaNothingMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("argument")) {
    argument = reqObj["argument"].as<String>();
    anyChanged = true;
  }
  if (anyChanged) configSave();
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaNothingMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/nothing", std::bind(&IotsaNothingMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/nothing", true, true);
  name = "nothing";
#endif
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
