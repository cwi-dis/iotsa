//
// Generic BLE-central example: discover, add, rename, and remove any number
// of named BLE devices entirely through IotsaBLEClientMod's own REST/web
// surface -- no app-specific protocol code needed at all. Each added device
// defaults to a plain IotsaRunmodeBLEClient (IotsaBLEClientMod::addDevice()'s
// own default), so if the target happens to be another iotsa device, the
// generic runmode commands (identify/reboot/promoteMode/setWifiDisabled)
// work too; if not (e.g. a non-iotsa BLE peripheral), you still get
// discovery/connect/disconnect and found/connected status for free, via
// IotsaBLEClientDevice's own IotsaApiModObject surface (cwi-dis/iotsa#268).
//
// Supersedes sandbox/BLEClient (a dual-role client+server debugging rig,
// moved to sandbox/ under #222) -- this is the modern "manage BLE devices
// generically" example the #264 collection-management generalization was
// built to demonstrate.
//
// See /bleclient for the web UI, or /api/bleclient for the REST equivalent.
//
#include "iotsa.h"
#include "iotsaWifi.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("Generic BLE Client");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEClient.h"
IotsaBLEClientMod bleClientMod(application);

//
// Boilerplate for iotsa server, with hooks to our code added.
//
void setup(void) {
  application.setup();
  application.lateSetup();
  // Scan for unknown devices continuously, so an operator can see what's
  // nearby before deciding what to add by name (see the web UI's "Available
  // Unknown/new" section).
  bleClientMod.findUnknownDevices(true);
}

void loop(void) {
  application.loop();
}
