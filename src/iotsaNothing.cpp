#include "iotsa.h"
#include "iotsaNothing.h"
#include "iotsaConfigFile.h"

void IotsaNothingMod::setup() {
  configLoad();
}

void IotsaNothingMod::lateSetup() {
  api.setup("nothing", true, true);
  name = "nothing";
}

void IotsaNothingMod::loop() {
}

void IotsaNothingMod::configLoad() {
  IotsaConfigFileLoad cf("/config/nothing.cfg");
  cf.get("argument", argument, "");
 
}

void IotsaNothingMod::configSave() {
  IotsaConfigFileSave cf("/config/nothing.cfg");
  cf.put("argument", argument);
}

bool IotsaNothingMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "argument", argument)) {
    anyChanged = true;
  }
  if (anyChanged) { configSave(); if (changed) *changed = true; }
  checkUnhandled(reqObj);
  return true;
}

bool IotsaNothingMod::getHandler(const char *path, JsonObject& reply) {
  reply["argument"] = argument;
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaNothingMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("argument", argument, IotsaWebContext::Saved);
}

void IotsaNothingMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.includesSettings()) {
    IotsaWebContext::Form form(ctx);
    ctx.showField("Argument", "argument", argument, IotsaWebContext::Saved);
  }
}

String IotsaNothingMod::info() {
  String message = "<p>Built with boilerplate module. See <a href=\"/nothing\">/nothing</a> to change the boilerplate module argument.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB
