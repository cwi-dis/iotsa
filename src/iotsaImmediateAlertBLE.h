#ifndef _IOTSAIMMEDIATEALERTBLE_H_
#define _IOTSAIMMEDIATEALERTBLE_H_
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

// Bluetooth SIG-adopted Immediate Alert Service (IAS) -- not iotsa-specific,
// a real standard GATT service (part of the "Find Me" profile: a client
// writes an alert level, the peripheral alerts at that intensity). Using
// the real SIG UUIDs means any generic BLE tool (nRF Connect, etc.)
// recognizes this service by name instead of showing an opaque custom one.
// https://www.bluetooth.com/specifications/specs/immediate-alert-service-1-0/
namespace IotsaImmediateAlertBLE {
  static constexpr UUIDstring serviceUUID    = "00001802-0000-1000-8000-00805f9b34fb";
  static constexpr UUIDstring alertLevelUUID = "00002a06-0000-1000-8000-00805f9b34fb";
  enum AlertLevel : uint8_t {
    NoAlert = 0,
    MildAlert = 1,
    HighAlert = 2,
  };
};

#endif // IOTSA_WITH_BLE
#endif
