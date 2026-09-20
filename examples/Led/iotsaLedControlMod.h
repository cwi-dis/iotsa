#ifndef _IOTSALEDCONTROLMOD_H_
#define _IOTSALEDCONTROLMOD_H_

#include "iotsa.h"
#include "iotsaLed.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#endif

//
// LED module: lets an external party trigger iotsaStatus's status-pulse channel
// (cwi-dis/iotsa#176) -- color, on/off duration, total duration -- over the
// web/REST/CoAP API, plus, when built with IOTSA_WITH_BLE, a solid-color-only BLE
// characteristic. A pulse is transient by design: it always decays back to the
// normal status display on its own, so there's nothing to read back or cancel
// (cwi-dis/iotsa#256). Folded together from the formerly-separate Led/BLELed
// examples, which had drifted into two different feature sets by accident rather
// than design -- see cwi-dis/iotsa#222.
//
class IotsaLedControlMod : public IotsaLedMod {
public:
  using IotsaLedMod::IotsaLedMod;
#ifdef IOTSA_WITH_BLE
  void setup() override;
#endif
  void lateSetup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
#ifdef IOTSA_WITH_BLE
  // BLE only ever triggers a solid color for a fixed duration -- there's no room
  // for separate on/off/duration parameters on a single characteristic write,
  // see cwi-dis/iotsa#222.
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "3B000001-1226-4A53-9D24-AFA50C0163A3";
  static constexpr UUIDstring rgbUUID = "3B000002-1226-4A53-9D24-AFA50C0163A3";
  static constexpr uint32_t bleSolidDurationMs = 5000;
#endif // IOTSA_WITH_BLE
};

#endif
