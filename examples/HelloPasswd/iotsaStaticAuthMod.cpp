#include "iotsaStaticAuthMod.h"

void IotsaStaticAuthMod::setup() {
}

void IotsaStaticAuthMod::serverSetup() {
}

void IotsaStaticAuthMod::loop() {
}

String IotsaStaticAuthMod::info() {
  return "";
}

bool IotsaStaticAuthMod::allows(const char *right) {
  if (!server->authenticate("admin", "admin")) {
    server->requestAuthentication();
    return false;
  }
  return true;
}

bool IotsaStaticAuthMod::allows(const char *obj, IotsaApiOperation verb) {
  return allows("api");
}
