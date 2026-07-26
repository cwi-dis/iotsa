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
#include "iotsaWifi.h"
#include "iotsaUser.h"
#include "iotsaHelloMod.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework

//
// Authentication class. Pass in default username, default password is
// set base on ESP8266 identity.
//
IotsaUserMod myAuthenticator(application, "owner");  // Our authenticator module

IotsaWifiMod wifiMod(application, &myAuthenticator);  // The network configuration module (authenticated)

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, &myAuthenticator);  // The over-the-air updater module (authenticated)
#endif

IotsaHelloMod helloMod(application, &myAuthenticator); // Our hello module (authenticated)

// Standard setup() method, hands off everything to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
