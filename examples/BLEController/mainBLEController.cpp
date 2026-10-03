//
// iotsa BLE device controller: discover, add, rename, and remove any number
// of other iotsa devices reachable over BLE, entirely through
// IotsaBLEClientCollectionMod's own REST/web surface -- no app-specific
// protocol code needed at all. Unlike the fully-generic collection mod
// itself, this filters candidates to iotsa devices specifically (anything
// advertising the universal runmode service every iotsa device compiles in
// unconditionally), so "Available Unknown/new devices" only ever shows
// other iotsa devices, not every random BLE gadget nearby.
//
// Each added device defaults to a plain IotsaRunmodeBLEClient
// (IotsaBLEClientMod::addDevice()'s own default), which now also exposes
// identify/reboot/promoteMode/setWifiDisabled as queueable commands via its
// own REST/web surface (IotsaRunmodeBLEClient::queueIdentify() & friends,
// serviced every tick by IotsaBLEClientCollectionMod::loop()) -- so this is
// a real "WiFi/REST to BLE-only iotsa device" bridge/gateway, the mirror
// image of HPS (cwi-dis/iotsa#267).
//
// Supersedes sandbox/BLEClient in spirit (a dual-role client+server
// debugging rig, moved there under #222) -- this is the modern replacement
// built on the cwi-dis/iotsa#268/#264 generic surface, not a from-scratch
// rewrite of that one (left as-is, still serves its own debugging purpose).
//
// See /bleclient for the web UI, or /api/bleclient for the REST equivalent.
//
#include "iotsa.h"
#include "iotsaWifi.h"

#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.

IotsaApplication application("iotsa BLE Controller");
IotsaWifiMod wifiMod(application);

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#include "iotsaBLEClientCollection.h"
#include "iotsaRunmodeBLEClient.h"

class BLEControllerMod : public IotsaBLEClientCollectionMod {
public:
  using IotsaBLEClientCollectionMod::IotsaBLEClientCollectionMod;
protected:
  // Only surface other iotsa devices as "unknown/addable" candidates --
  // every iotsa device compiles IotsaRunmodeMod in unconditionally, so this
  // is the generic recognition signal (same one
  // IotsaRunmodeBLEClient's own header comment describes).
  bool isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) override {
    return device->isAdvertisingService(NimBLEUUID(IotsaRunmodeBLE::serviceUUID));
  }
};

BLEControllerMod bleClientMod(application);

void setup(void) {
  application.setup();
  application.lateSetup();
  // Scan for unknown (iotsa) devices continuously, so an operator can see
  // what's nearby before deciding what to add by name.
  bleClientMod.findUnknownDevices(true);
  // Known devices are only looked for when a command is queued for them (cwi-dis/iotsa#263).
  bleClientMod.setScanOnlyForPendingWork(true);
}

void loop(void) {
  application.loop();
}
