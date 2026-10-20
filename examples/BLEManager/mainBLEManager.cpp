//
// A "BLE Manager" server: manages other iotsa devices over BLE. On the web
// page /bleclient, or with /api/bleclient:
//
// - Scan shows the iotsa devices nearby, for a while (20 seconds by default).
// - Add one by its name; the manager remembers it over reboots.
// - For an added device: identify it, reboot it, switch its WiFi on or off,
//   or confirm a mode change it is waiting for (configuration or OTA mode,
//   requested on the device itself; LedBattery shows how a device allows it).
//
// So it is a bridge: you reach it over WiFi, it reaches devices that only
// have BLE. See iotsaBLEManagerMod.cpp.
//

#include "iotsa.h"
#include "iotsaBLEManagerMod.h"

#ifndef IOTSA_WITH_BLE
#error This example needs BLE: build with -DIOTSA_WITH_BLE
#endif

// The application: handles WiFi, OTA and the other standard modules for you.
IotsaApplication application("iotsa BLE Manager");

// Our module, see iotsaBLEManagerMod.cpp.
IotsaBLEManagerMod bleClientMod(application);

// Standard setup() method, hands off most work to the application framework
void setup(void){
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
