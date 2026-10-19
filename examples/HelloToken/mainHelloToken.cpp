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

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework

// Authentication #1: users.
IotsaUserMod myUserAuthenticator(application);

// Authentication #2: tokens, falling back to the users when a request has none.
IotsaStaticTokenMod myTokenAuthenticator(application, myUserAuthenticator);

IotsaHelloMod helloMod(application); // Our hello module

// Standard setup() method, hands off everything to the application framework
void setup(void){
  application.setAuth(&myTokenAuthenticator);  // every module, the standard ones included, uses this
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
