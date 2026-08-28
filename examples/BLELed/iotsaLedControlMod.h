#ifndef _IOTSALEDCONTROLMOD_H_
#define _IOTSALEDCONTROLMOD_H_

#include "iotsa.h"
#include "iotsaLed.h"
#include "iotsaBLEServer.h"

//
// LED module.
//
class IotsaLedControlMod : public IotsaLedMod {
public:
  using IotsaLedMod::IotsaLedMod;
  void setup() override;
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
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "3B000001-1226-4A53-9D24-AFA50C0163A3";
  static constexpr UUIDstring rgbUUID = "3B000002-1226-4A53-9D24-AFA50C0163A3";
#endif // IOTSA_WITH_BLE

};

#endif
