#ifndef _IOTSARUNMODE_H_
#define _IOTSARUNMODE_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

//
// IotsaRunmodeMod -- the external control surface onto IotsaController
// (cwi-dis/iotsa#106, docs/controller-architecture.md).
//
// Core-tier: unconditionally ensure()d by IotsaApplication::setup(), the same
// treatment as IotsaConfigMod -- never an optional add. It is the one place,
// across every transport (REST/web now, BLE next), that a client steers the
// device's *operating state*: request a maintenance mode for the next boot,
// reboot, toggle the WiFi / BLE radios at runtime.
//
// Every handler here is thin glue: a call into iotsaController. The mode /
// reboot / radio keys also still appear in /api/config as [[deprecated]]
// forwarders for one release, so existing scripts and the Python CLI keep
// working until they move to /api/runmode (see the "Transition strategy"
// section of docs/controller-architecture.md).
//
class IotsaRunmodeMod : public IotsaModule, public IotsaSingletonModule<IotsaRunmodeMod> {
public:
  IotsaRunmodeMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaModule(_app, _auth, true)   // early: mode/reboot control belongs up before the app modules
  {
    claimSingleton(this);
  }
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
#ifdef IOTSA_WITH_BLE
  // The BLE control service: read the current mode, request a mode for the next
  // boot, reboot. A client that discovers an iotsa device over BLE also wants to
  // steer it (cwi-dis/iotsa#106, #233). Writes are stashed here and acted on
  // from loop() -- blePutHandler runs in the NimBLE host task.
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  IotsaBleApiService bleApi;
  int _pendingBleMode = -1;       // -1: nothing pending; else an iotsa_mode value
  bool _pendingBleReboot = false;
  // Minted for iotsa#106 -- the iotsa runmode control service. xxxx0001 is the
  // service, xxxx0002+ the characteristics (same convention as elsewhere).
  static constexpr UUIDstring serviceUUID       = "6E5D0001-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring currentModeUUID   = "6E5D0002-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring requestedModeUUID = "6E5D0003-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring rebootUUID        = "6E5D0004-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
#endif // IOTSA_WITH_BLE
};

#endif
