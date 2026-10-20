//
// The Hello example with a password: changing the name needs a login with
// the fixed username "admin" and password "admin" (see iotsaStaticAuthMod.cpp).
// The login protects the whole device: the WiFi settings, over-the-air
// updates and so on need it too.
//

#include "iotsa.h"
#include "iotsaStaticAuthMod.h"
#include "iotsaHelloMod.h"

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Hello World Server");

// Our authenticator, see iotsaStaticAuthMod.cpp.
IotsaStaticAuthMod myAuthenticator(application);

// Our module, the same as in HelloIotsa. It doesn't know about the password.
IotsaHelloMod helloMod(application);

// Standard setup() method, hands off everything to the application framework
void setup(void){
  // This is the line that makes the password work: from now on every module,
  // the standard ones included, asks myAuthenticator before a change.
  application.setAuth(&myAuthenticator);
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
