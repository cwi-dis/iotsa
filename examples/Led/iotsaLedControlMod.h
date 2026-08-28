#ifndef _IOTSALEDCONTROLMOD_H_
#define _IOTSALEDCONTROLMOD_H_

#include "iotsa.h"
#include "iotsaLed.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#endif

//
// LED module: full flash-pattern control (color, on/off duration, repeat count) over
// the web/REST/CoAP API, plus, when built with IOTSA_WITH_BLE, a solid-color-only BLE
// characteristic. Folded together from the formerly-separate Led/BLELed examples, which
// had drifted into two different feature sets by accident rather than design -- see
// cwi-dis/iotsa#222.
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
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
#ifdef IOTSA_WITH_BLE
  // BLE only ever sets/reports a solid color -- the flash-pattern parameters
  // (onDuration/offDuration/count) are REST/web-only, see cwi-dis/iotsa#222.
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "3B000001-1226-4A53-9D24-AFA50C0163A3";
  static constexpr UUIDstring rgbUUID = "3B000002-1226-4A53-9D24-AFA50C0163A3";
#endif // IOTSA_WITH_BLE
};

#endif
