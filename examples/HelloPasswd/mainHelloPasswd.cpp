//
// The Hello example with a password: changing the name needs a login with
// the fixed username "admin" and password "admin" (see iotsaStaticAuthMod.cpp).
// The login protects the whole device: the WiFi settings, over-the-air
// updates and so on need it too.
//

#include "iotsa.h"
#include "iotsaStaticAuthMod.h"
#include "iotsaHelloMod.h"

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework
IotsaStaticAuthMod myAuthenticator(application);  // Our authenticator module

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
