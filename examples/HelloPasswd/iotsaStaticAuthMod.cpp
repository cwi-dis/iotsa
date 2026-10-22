#include "iotsaStaticAuthMod.h"

// An authenticator decides whether a request may do something. It is a module
// like any other, but mainHelloPasswd.cpp also hands it to the application with
// application.setAuth(): from then on every module asks it, the standard ones
// included (WiFi settings, over-the-air updates, ...).
//
// Iotsa comes with a real one, IotsaUserMod (see HelloUser), which keeps a list
// of users with passwords and rights. This one is as simple as it gets: one
// fixed username and password, and whoever knows them may do everything.

// Nothing to set up and nothing to do in the loop, but every module needs these.
void IotsaStaticAuthMod::setup() {
}

void IotsaStaticAuthMod::loop() {
}

// Called when a request wants to do something that needs the right "right"
// (such as changing the name on the /hello page), or, with right nullptr, a
// REST call (such as a PUT on /api/hello). This authenticator doesn't look at
// the right, only at the login: whoever sent "admin" and "admin" may do it,
// anybody else would need to log in. The framework asks the browser for the
// login, this method only answers.
IotsaAccess IotsaStaticAuthMod::allows(IotsaContext& ctx, const char *right) {
  if (ctx.hasBasicAuth("admin", "admin")) return IotsaAccess::Granted;
  return IotsaAccess::NeedsCredentials;
}
