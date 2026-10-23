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

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Hello World Server");

// Our authenticator: the users, with their passwords and rights. It is a
// standard module, and it has its own page, /users.
IotsaUserMod myAuthenticator(application);

// Our module, the same as in HelloIotsa except that it asks for the right
// "hello", see iotsaHelloMod.cpp.
IotsaHelloMod helloMod(application);

// Standard setup() method, hands off everything to the application framework
void setup(void){
  // This is the line that makes the users work: from now on every module,
  // the standard ones included, asks myAuthenticator before a change.
  application.setAuth(&myAuthenticator);
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
