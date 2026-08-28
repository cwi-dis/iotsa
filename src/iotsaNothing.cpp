#include "iotsa.h"
#include "iotsaNothing.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_WEB
void
IotsaNothingMod::webHandler() {
  bool anyChanged = false;
  if( api.webService->server->hasArg("argument")) {
    if (needsAuthentication()) return;
    argument = api.webService->server->arg("argument");
    anyChanged = true;
  }
  if (anyChanged) configSave();

  String message = "<html><head><title>Boilerplate module</title></head><body><h1>Boilerplate module</h1>";
  message += "<form method='get'>Argument: <input name='argument' value='";
  message += htmlEncode(argument);
  message += "'><br><input type='submit'></form>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaNothingMod::info() {
  String message = "<p>Built with boilerplate module. See <a href=\"/nothing\">/nothing</a> to change the boilerplate module argument.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

void IotsaNothingMod::setup() {
  configLoad();
}

bool IotsaNothingMod::getHandler(const char *path, JsonObject& reply) {
  reply["argument"] = argument;
  return true;
}

bool IotsaNothingMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "argument", argument)) {
    anyChanged = true;
  }
  if (anyChanged) configSave();
  checkUnhandled(reqObj);
  return anyChanged;
}

void IotsaNothingMod::lateSetup() {
  api.setup("nothing", true, true);
  name = "nothing";
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
