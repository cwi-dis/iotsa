//
// A "BLE Ringer" server: the Ringer example, but rung over BLE by a
// BLEButton instead of over the network. It shows the alert on the board's
// status LED rather than sounding a buzzer, so it needs no extra hardware.
// See iotsaBLERingerMod.cpp.
//
// Ringing uses the Immediate Alert Service, a Bluetooth standard (the "Find
// Me" profile), so a generic BLE tool such as nRF Connect can ring it too.
//
// The board is an ESP32-C3-DevKitM-1, whose RGB LED is the status LED.
//

#include "iotsa.h"
#include "iotsaBLERingerMod.h"

#ifndef IOTSA_WITH_BLE
#error This example needs BLE: build with -DIOTSA_WITH_BLE
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
// It also creates the status LED, because the board has one.
IotsaApplication application("BLE Ringer");

// Our module, see iotsaBLERingerMod.cpp.
IotsaBLERingerMod ringerMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
