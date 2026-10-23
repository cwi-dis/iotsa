#ifndef _IOTSAIMMEDIATEALERTBLE_H_
#define _IOTSAIMMEDIATEALERTBLE_H_
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

// The Immediate Alert Service (IAS), shared by BLEButton and BLERinger; the
// two copies are identical. It is a Bluetooth standard, not iotsa's own: a
// client writes an alert level, the device alerts. Because it is standard,
// generic BLE tools such as nRF Connect know it by name.
// https://www.bluetooth.com/specifications/specs/immediate-alert-service-1-0/
//
// The UUIDs are the short 16-bit form, which takes 2 bytes in an
// advertisement instead of 16.
namespace IotsaImmediateAlertBLE {
  static constexpr UUIDstring serviceUUID    = "1802";
  static constexpr UUIDstring alertLevelUUID = "2A06";
  enum AlertLevel : uint8_t {
    NoAlert = 0,
    MildAlert = 1,
    HighAlert = 2,
  };
};

#endif // IOTSA_WITH_BLE
#endif
