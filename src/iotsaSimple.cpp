#include "iotsaSimple.h"


void IotsaSimpleMod::setup() {
  // Handled in client application, in normal setup() function
}

void IotsaSimpleMod::serverSetup() {
#ifdef WITH_HTTP_OR_HTTPS
  server->on(url, hfun);
#endif
}

void IotsaSimpleMod::loop() {
  // Handled in client application, in normal loop() function
}

String IotsaSimpleMod::info() {
  if (ifun) {
  	return ifun();
  }
  return "";
}
