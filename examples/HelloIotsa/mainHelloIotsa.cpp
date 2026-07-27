//
// Reference example for how we think an iotsa application should be structured:
// app-specific module logic lives in its own .h/.cpp pair (iotsaHelloMod.h/.cpp),
// following the same declaration/implementation split used by iotsa's own core
// modules. This file only contains the application glue: which modules to
// instantiate, and setup()/loop().
//
// Functionally this merges what used to be two separate examples: HelloCpp
// (the "hello, name" module implemented as a C++ class) and HelloApi (the same,
// with a REST API for changing the name added on top).
//

#include "iotsa.h"
#include "iotsaWifi.h"
#include "iotsaHelloMod.h"

// CHANGE: Add application includes and declarations here

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Iotsa Hello World Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

IotsaHelloMod helloMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.serverSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
