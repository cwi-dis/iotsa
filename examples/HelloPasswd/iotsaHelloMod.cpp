#include "iotsaHelloMod.h"
#include "iotsaConfigFile.h"

// Implementation of the Hello module. This is the same module as in
// HelloIotsa: the password check is not here but in iotsaStaticAuthMod.cpp.
//
// An iotsa module is somewhat like an Arduino sketch, except (1) it provides
// a way for the "outside world" to see and change values (either through a
// web page or programmatically through REST or other protocols), and (2) there
// are various other modules available, that handle WiFi, over-the-air
// programming, and much more. Some come automatically, others you add in
// mainHelloPasswd.cpp, which is also where this module is created.

// setup() handles early initialization of this module. Think of the same type
// of things you would do in an Arduino sketch setup(): opening GPIO ports, etc.
// Here we load the name we saved earlier.
void IotsaHelloMod::setup() {
  configLoad();
}

// An extension of the Arduino setup() idea: first setup() is called for all
// modules, then lateSetup(). Here we register the web page and the API:
// api.setup("hello", true, true) gives GET and PUT on /api/hello, and the
// web page /hello. Apart from that you rarely need lateSetup(), unless your
// module depends on other modules already being set up.
void IotsaHelloMod::lateSetup() {
  api.setup("hello", true, true);
  name = "hello";
}

// Called all the time, like the Arduino loop(). Don't block.
void IotsaHelloMod::loop() {
  // Nothing to do in the loop, for this module
}

// The name is kept in a file, so it survives a reboot. configLoad() reads it,
// with "human" if there is no file yet; configSave() writes it.
void IotsaHelloMod::configLoad() {
  IotsaConfigFileLoad cf("/config/hello.cfg");
  cf.get("personName", personName, "human");
}

void IotsaHelloMod::configSave() {
  IotsaConfigFileSave cf("/config/hello.cfg");
  cf.put("personName", personName);
}

// The API PUT handler (REST or other protocols): pick up the
// values the caller sent, and store or use them.
bool IotsaHelloMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "personName", personName)) {
    // The request set the name, so save it.
    configSave();
    if (changed) *changed = true;
  }
  // Anything else in the request is reported back as "iotsa_api_ignored".
  checkUnhandled(reqObj);
  // Success, also when nothing changed. On an error, return apiError(status, "why").
  return true;
}

// The API GET handler: the values you want to send back to the caller.
bool IotsaHelloMod::getHandler(const char *path, JsonObject& reply) {
  reply["personName"] = personName;
  return true;
}

#ifdef IOTSA_WITH_WEB // only with web pages
// The web page: the iotsa framework and the ctx object handle the common
// things (page, feedback, authentication).
//
// formHandler_args() handles what the form sent: here you pick up the values
// and store or use them. Similar to putHandler.
void IotsaHelloMod::formHandler_args(IotsaWebContext& ctx) {
  // Config: when the name changes, the framework calls configSave().
  ctx.arg("personName", personName, IotsaWebContext::Config);
}

// formHandler_fields() writes the page body, you implement here what values
// you want to send back to the user. Similar to getHandler.
// Called twice: once for the everyday part of the page, once for the
// configuration part (ctx.section). Hello only has an everyday part.
void IotsaHelloMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section != IotsaWebContext::Everyday) return;
  ctx << "<p>Hello, " + htmlEncode(personName) + "!</p>\n";
  IotsaWebContext::Form form(ctx, "Set");
  ctx.field("Name", "personName", personName, IotsaWebContext::Config);
}
#endif // IOTSA_WITH_WEB

// Return some information about this module, for the main page of the web server.
String IotsaHelloMod::info() {
  String rv = "<p>See <a href=\"/hello\">/hello</a> to change the name ";
  rv += htmlEncode(personName);
  rv += " that is currently greeted.";
  rv += "</p>";
  return rv;
}
