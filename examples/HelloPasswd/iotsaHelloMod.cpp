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
  // Changing the greeting needs a login (the same rule as a PUT on
  // /api/hello); just looking doesn't.
  // Live: the greeting isn't saved, it is gone after a reboot.
  ctx.arg("greeting", greeting, IotsaWebContext::Live);
}

void IotsaHelloMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  if (greeting != "") ctx << "<p>Hello, " + htmlEncode(greeting) + "!</p>\n";
  IotsaWebContext::Form form(ctx, "Set");
  ctx.field("Greeting", "greeting", greeting, IotsaWebContext::Live);
}
#endif // IOTSA_WITH_WEB

bool IotsaHelloMod::getHandler(const char *path, JsonObject& reply) {
  reply["greeting"] = greeting;
  return true;
}

bool IotsaHelloMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  JsonObject reqObj = request.as<JsonObject>();
  getFromRequest<const char *>(reqObj, "greeting", greeting);
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
