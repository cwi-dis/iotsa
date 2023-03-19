#include "iotsa.h"
#include "iotsaBLEREST.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_WEB

String IotsaBLERestMod::info() {
  String message = "<p>Built with support for REST over Bluetooth LE.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaBLERestMod::getHandler(const char *path, JsonObject& reply) {
    return true;
}

bool IotsaBLERestMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
    return false;
}

void IotsaBLERestMod::setup() {
}

void IotsaBLERestMod::serverSetup() {
  api.setup("/api/blerest", true, false);
  name = "blerest";
}

void IotsaBLERestMod::loop() {
}
