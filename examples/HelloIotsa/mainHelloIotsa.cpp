//
// Reference example for how we think an iotsa application should be structured:
// app-specific module logic lives in its own .h/.cpp pair (iotsaHelloMod.h/.cpp),
// following the same declaration/implementation split used by iotsa's own core
// modules. This file only contains the application glue: which modules to
// instantiate, and setup()/loop().
//

#include "iotsa.h"
#include "iotsaHelloMod.h"

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("Iotsa Hello World Server");

// Our module, see iotsaHelloMod.cpp. Creating it adds it to the application.
IotsaHelloMod helloMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
