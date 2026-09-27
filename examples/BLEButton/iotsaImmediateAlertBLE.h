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
//
// Short (16-bit) form, not the full 128-bit Bluetooth-Base-UUID string --
// NimBLEUUID's string constructor treats a 4-hex-digit string as a real
// 16-bit UUID (2 bytes on the wire, vs 16 for the 128-bit form), matching
// how iotsaApiHps.h's own SIG-assigned UUIDs are already written. Every
// byte counts in the advertisement/scan-response payload (see
// IotsaRunmodeMod::setup()'s comment on why registration order matters
// there too) -- this is the other half of making that budget less tight.
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
