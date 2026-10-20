#ifndef _IOTSABLEMANAGERMOD_H_
#define _IOTSABLEMANAGERMOD_H_

#include "iotsa.h"
#include "iotsaBLEClientManager.h"
#include "iotsaRunmodeBLEClient.h"

//
// BLE manager module. The standard BLE client manager (IotsaBLEClientManagerMod)
// does all the work: scanning, adding, renaming and removing devices, on the
// web page /bleclient and with /api/bleclient. This subclass only decides
// which devices are interesting: other iotsa devices.
//
class IotsaBLEManagerMod : public IotsaBLEClientManagerMod {
public:
  using IotsaBLEClientManagerMod::IotsaBLEClientManagerMod;
protected:
  const char *devicesLabel() override { return "iotsa BLE devices"; }
  bool isInterestingUnknownDevice(const NimBLEAdvertisedDevice* device) override;
};

#endif
