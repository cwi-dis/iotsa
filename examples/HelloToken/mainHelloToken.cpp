//
// The Hello example with users and tokens: changing the name needs a login by
// a user with the right "hello", or a request carrying a token with that right
// (see iotsaHelloMod.cpp). Tokens are for programs rather than people: they
// are added on the /tokens page, each with its own rights.
//
// Users are managed on the /users page, in configuration mode, as in
// HelloUser.
//

#include "iotsa.h"
#include "iotsaUser.h"
#include "iotsaStaticToken.h"
#include "iotsaHelloMod.h"

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Hello World Server");

// The users, as in HelloUser.
IotsaUserMod myUserAuthenticator(application);

// The tokens, see iotsaStaticToken.cpp. A request without a valid token is
// passed on to the users.
IotsaStaticTokenMod myTokenAuthenticator(application, myUserAuthenticator);

// Our module, the same as in HelloUser.
IotsaHelloMod helloMod(application);

// Standard setup() method, hands off everything to the application framework
void setup(void){
  // Every module, the standard ones included, asks the tokens first, and
  // through them the users.
  application.setAuth(&myTokenAuthenticator);
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
