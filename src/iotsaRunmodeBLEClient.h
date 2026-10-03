#ifndef _IOTSARUNMODEBLECLIENT_H_
#define _IOTSARUNMODEBLECLIENT_H_
#include "iotsaBLEClientDevice.h"
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

// Client-side counterpart to IotsaRunmodeMod's BLE control surface. Every
// iotsa device compiles IotsaRunmodeMod in unconditionally (it's core, not
// optional), so any remote that answers on IotsaRunmodeBLE::serviceUUID is,
// by definition, an iotsa BLE server -- that's the generic recognition signal
// a caller can use (e.g. IotsaBLEClientCollectionMod::isInterestingUnknownDevice())
// instead of filtering on a specific app's own protocol. Adds typed methods over
// IotsaBLEClientDevice's generic get()/set() so a caller doesn't have to
// hand-roll calls against raw IotsaRunmodeBLE UUIDs itself. Any BLE client
// connection to an iotsa device can use these directly (e.g. Lissabon's
// DimmerBLEClient inherits from this to get them for free, alongside its own
// dimmer-specific protocol).
class IotsaRunmodeBLEClient : public IotsaBLEClientDevice {
public:
  using IotsaBLEClientDevice::IotsaBLEClientDevice;
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
  //
  // Asynchronous counterparts to the four methods above (not getCurrentMode/
  // requestMode -- those stay the caller's own direct, already-connected
  // responsibility): queue the command for whenever this device is next
  // reachable and connected, instead of requiring the caller to already be
  // connected. The base class's connection state machine (requestWork()/
  // doWork(), cwi-dis/iotsa#263) does the connect/fire/disconnect; outcome
  // in getHandler()'s lastWorkStatus. Only one command may be queued at a
  // time; each returns false if one already is.
  bool queueIdentify();
  bool queueReboot();
  bool queuePromoteMode();
  bool queueSetWifiDisabled(bool disabled);
  bool putHandler(const JsonVariant& request) override;
#ifdef IOTSA_WITH_WEB
  void formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) override;
  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
#endif
protected:
  bool doWork() override;
  void workAbandoned() override;
  enum class PendingCommand { None, Identify, Reboot, PromoteMode, SetWifiDisabled };
  bool queueCommand(PendingCommand cmd);
  PendingCommand pendingCommand = PendingCommand::None;
  bool pendingSetWifiDisabledValue = false;
  static const uint32_t commandTimeoutMillis = 10000;
};

#endif // IOTSA_WITH_BLE
#endif
