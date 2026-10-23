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

void IotsaNothingMod::putHandler(IotsaApiContext& ctx) {
  ctx.arg("argument", argument, IotsaApiContext::Saved);
}

void IotsaNothingMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("argument", argument);
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
