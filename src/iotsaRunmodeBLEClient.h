#ifndef _IOTSARUNMODEBLECLIENT_H_
#define _IOTSARUNMODEBLECLIENT_H_
#include "iotsaBLEClientConnection.h"
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

// Client-side counterpart to IotsaRunmodeMod's BLE control surface. Every
// iotsa device compiles IotsaRunmodeMod in unconditionally (it's core, not
// optional), so any remote that answers on IotsaRunmodeBLE::serviceUUID is,
// by definition, an iotsa BLE server -- that's the generic recognition signal
// a caller can use (e.g. IotsaBLEClientMod::setServiceFilter()) instead of
// filtering on a specific app's own protocol. Adds typed methods over
// IotsaBLEClientConnection's generic get()/set() so a caller doesn't have to
// hand-roll calls against raw IotsaRunmodeBLE UUIDs itself. Any BLE client
// connection to an iotsa device can use these directly (e.g. Lissabon's
// DimmerBLEClient inherits from this to get them for free, alongside its own
// dimmer-specific protocol).
class IotsaRunmodeBLEClient : public IotsaBLEClientConnection {
public:
  using IotsaBLEClientConnection::IotsaBLEClientConnection;
  bool getCurrentMode(uint8_t& mode);
  bool requestMode(uint8_t mode);
  bool reboot();
  // Applies whatever mode was last set via requestMode() immediately, instead
  // of waiting for the next boot -- the remote treats a BLE connection as
  // proof of physical presence (see IotsaRunmodeMod::allowBLEModeSwitch()).
  // A remote that hasn't opted in via allowBLEModeSwitch() silently ignores
  // this; there is no separate way to tell the two cases apart from here.
  bool promoteMode();
  bool setWifiDisabled(bool disabled);
  bool identify();
};

#endif // IOTSA_WITH_BLE
#endif
