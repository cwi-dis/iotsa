#include "iotsaHelloMod.h"

// Implementation of the Hello module
void IotsaHelloMod::setup() {
  // Nothing to do during early initialization for this module
}

#ifdef IOTSA_WITH_WEB
// The web page: formHandler_args() handles what the form sent, then
// formHandler_fields() writes the page. The framework does the rest (page,
// feedback, authentication), see cwi-dis/iotsa#183.
void IotsaHelloMod::formHandler_args(IotsaWebContext& ctx) {
  // Changing the name needs the right "greeting"; just looking doesn't.
  ctx.needsRight("greeting");
  // Live: the name isn't saved, it is gone after a reboot.
  ctx.arg("personName", personName, IotsaWebContext::Live);
}

void IotsaHelloMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx << "<p>Hello, " + htmlEncode(personName) + "!</p>\n";
  IotsaWebContext::Form form(ctx, "Set");
  ctx.field("Name", "personName", personName, IotsaWebContext::Live);
}
#endif // IOTSA_WITH_WEB

bool IotsaHelloMod::getHandler(const char *path, JsonObject& reply) {
  reply["personName"] = personName;
  return true;
}

bool IotsaHelloMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  JsonObject reqObj = request.as<JsonObject>();
  getFromRequest<const char *>(reqObj, "personName", personName);
  // Anything else in the request is reported back as "iotsa_api_ignored".
  checkUnhandled(reqObj);
  // Success, also when nothing changed. On an error, return apiError(status, "why").
  return true;
}

void IotsaHelloMod::lateSetup() {
  // Setup the web server hooks for this module.
  api.setup("hello", true, true);
  name = "hello";
}

String IotsaHelloMod::info() {
  // Return some information about this module, for the main page of the web server.
  String rv = "<p>See <a href=\"/hello\">/hello</a> to change the name ";
  rv += htmlEncode(personName);
  rv += " that is currently greeted.";
  rv += "</p>";
  return rv;
}

void IotsaHelloMod::loop() {
  // Nothing to do in the loop, for this module
}
