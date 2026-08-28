#include "iotsaStaticAuthMod.h"

void IotsaStaticAuthMod::setup() {
}

void IotsaStaticAuthMod::lateSetup() {
}

void IotsaStaticAuthMod::loop() {
}

String IotsaStaticAuthMod::info() {
  return "";
}

bool IotsaStaticAuthMod::allows(const char *right) {
  if (!app.server->authenticate("admin", "admin")) {
    app.server->requestAuthentication();
    return false;
  }
  return true;
}

bool IotsaStaticAuthMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}
