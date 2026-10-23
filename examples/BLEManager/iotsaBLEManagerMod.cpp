#include "iotsaBLEManagerMod.h"

// Implementation of the BLE manager module. Like Led, it extends a standard
// module, and calls nothing of its own: the manager asks it questions.

// A scan finds every BLE device nearby. Only show the iotsa devices: every
// iotsa device with BLE advertises the runmode service.
bool IotsaBLEManagerMod::isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) {
  return device->isAdvertisingService(NimBLEUUID(IotsaRunmodeBLE::serviceUUID));
}
