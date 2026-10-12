//
// iotsa BLE device manager: discover, add, rename, and remove any number
// of other iotsa devices reachable over BLE, entirely through
// IotsaBLEClientManagerMod's own REST/web surface -- no app-specific
// protocol code needed at all. Unlike the fully-generic collection mod
// itself, this filters candidates to iotsa devices specifically (anything
// advertising the universal runmode service every iotsa device compiles in
// unconditionally), so "Available Unknown/new devices" only ever shows
// other iotsa devices, not every random BLE gadget nearby.
//
// Each added device is a plain IotsaRunmodeBLEClient (the default factory of
// the manager's IotsaBLEClientCollection), and is remembered across reboots
// (cwi-dis/iotsa#286). It also exposes
// identify/reboot/promoteMode/setWifiDisabled as queueable commands via its
// own REST/web surface (IotsaRunmodeBLEClient::queueIdentify() & friends,
// serviced every tick by IotsaBLEClientMod::loop()) -- so this is
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

IotsaApplication application("iotsa BLE Manager");

#include "iotsaBLEClientManager.h"
#include "iotsaRunmodeBLEClient.h"

class BLEManagerMod : public IotsaBLEClientManagerMod {
public:
  using IotsaBLEClientManagerMod::IotsaBLEClientManagerMod;
protected:
  const char *devicesLabel() override { return "iotsa BLE devices"; }
  // Only surface other iotsa devices as "unknown/addable" candidates --
  // every iotsa device compiles IotsaRunmodeMod in unconditionally, so this
  // is the generic recognition signal (same one
  // IotsaRunmodeBLEClient's own header comment describes).
  bool isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) override {
    return device->isAdvertisingService(NimBLEUUID(IotsaRunmodeBLE::serviceUUID));
  }
};

BLEManagerMod bleClientMod(application);

void setup(void) {
  application.setup();
  application.lateSetup();
  // Not scanning for unknown devices continuously (that kept the radio
  // scanning forever): an operator starts a session with scanUnknown, on the
  // web page or over REST, to see what's nearby before adding it by name.
}

void loop(void) {
  application.loop();
}
