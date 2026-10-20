#include "iotsaSimple.h"


void IotsaSimpleMod::setup() {
  // Handled in client application, in normal setup() function
}

void IotsaSimpleMod::lateSetup() {
#ifdef IOTSA_HAS_WEBSERVER
  // Connection: close, as for every page (cwi-dis/iotsa#293).
  app.server->on(url, [this]() { iotsaNoKeepAlive(app.server); hfun(); });
#endif
}

void IotsaSimpleMod::loop() {
  // Handled in client application, in normal loop() function
}

#ifdef IOTSA_WITH_WEB
String IotsaSimpleMod::info() {
  if (ifun) {
  	return ifun();
  }
  return "";
}
#endif