//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
// A "hello" module is added, which greets the user with a name settable through
// a web form (not kept over reboots).
//
// This version requires a username/password to change the greeting, to enable
// over-the-air updating and to change the WiFi configuration.
// The usernames/password combination is changeable.
//

#include "iotsa.h"
#include "iotsaUser.h"
#include "iotsaHelloMod.h"

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework

//
// Authentication class. Pass in default username, default password is
// set base on ESP8266 identity.
//
IotsaUserMod myAuthenticator(application, "owner");  // Our authenticator module

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
