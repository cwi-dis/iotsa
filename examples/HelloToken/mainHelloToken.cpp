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
#include "iotsaStaticToken.h"
#include "iotsaHelloMod.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

//
// Instantiate all the objects we need.
//
IotsaApplication application("Iotsa Hello World Server"); // The application framework

//
// Authentication class #1, user-based. Pass in default username, default password is
// set base on ESP8266 identity.
//
IotsaUserMod myUserAuthenticator(application, "owner");  // Our authenticator module

//
// Authentication class #2, token based. The user can add static tokens with specific rights.
//
IotsaStaticTokenMod myTokenAuthenticator(application, myUserAuthenticator);

IotsaWifiMod wifiMod(application, &myUserAuthenticator);  // The network configuration module (authenticated by user only)

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application, &myUserAuthenticator);  // The over-the-air updater module (authenticated by user only)
#endif

IotsaHelloMod helloMod(application, &myTokenAuthenticator); // Our hello module (authenticated by user or token)

// Standard setup() method, hands off everything to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off everything to the application framework
void loop(void){
  application.loop();
}
