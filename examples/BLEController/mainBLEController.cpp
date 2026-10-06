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
// Replaces sandbox/BLEClient (a dual-role client+server debugging rig,
// retired in cwi-dis/iotsa#273): built on the cwi-dis/iotsa#268/#264 generic
// surface, and it took over that sandbox's build coverage (Arduino CI,
// classic ESP32/S3/lolin32, and a debug variant).
//
// See /bleclient for the web UI, or /api/bleclient for the REST equivalent.
//
#include "iotsa.h"

IotsaApplication application("iotsa BLE Controller");

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
}

void loop(void) {
  application.loop();
}
