//
// This server includes the wifi configuration module, and optionally the
// Over-The-Air update module (to allow uploading new code into the esp12 (or other
// board) from the Arduino IDE.
//
// A "hello" module is added, which greets the user with a name settable through
// a web form (not kept over reboots).
//

#include "iotsa.h"
#include "iotsaMultiUser.h"
#include "iotsaWifi.h"
#include "iotsaHelloMod.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server with API and multiple users");

// Multi-user access module. Defaults to all access until users are added
IotsaMultiUserMod myAuthenticator(application);  // Our authenticator module

IotsaWifiMod wifiMod(application, &myAuthenticator);  // The network configuration module (authenticated)

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, &myAuthenticator);
#endif

// Instantiate the Hello module, and install it in the framework
IotsaHelloMod helloMod(application, &myAuthenticator);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
