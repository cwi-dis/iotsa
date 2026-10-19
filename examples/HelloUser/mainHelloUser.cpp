//
// The Hello example with users: changing the name needs a login by a user
// with the right "hello" (see iotsaHelloMod.cpp). The users protect the whole
// device: the WiFi settings, over-the-air updates and so on need a login too.
//
// Users are managed on the /users page, in configuration mode. Until the
// first user is added everything is allowed. Give a user the rights "*" for
// everything, or "/hello/" for only changing the name.
//

#include "iotsa.h"
#include "iotsaUser.h"
#include "iotsaHelloMod.h"

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework

IotsaUserMod myAuthenticator(application);  // Our users

IotsaHelloMod helloMod(application); // Our hello module (authenticated)

// Standard setup() method, hands off everything to the application framework
void setup(void){
  application.setAuth(&myAuthenticator);  // every module, the standard ones included, uses this
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
