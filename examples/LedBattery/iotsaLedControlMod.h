#ifndef _IOTSALEDCONTROLMOD_H_
#define _IOTSALEDCONTROLMOD_H_

#include "iotsa.h"
#include "iotsaLed.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#endif

//
// LED control module. Extends the standard status LED module (IotsaLedMod)
// so the outside world can flash the LED: a colour, on and off times, and how
// long. On the web page /led, with PUT /api/led (REST or CoAP), and, built with
// IOTSA_WITH_BLE, with a BLE service of its own (solid colour only).
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
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
#ifdef IOTSA_WITH_BLE
  bool blePutHandler(UUIDstring charUUID) override;
#endif
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Led Server"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
#ifdef IOTSA_WITH_BLE
  // Our BLE service: one characteristic, rgb, that a client writes.
  IotsaBleApiService bleApi;
  static constexpr UUIDstring serviceUUID = "3B000001-1226-4A53-9D24-AFA50C0163A3";
  static constexpr UUIDstring rgbUUID = "3B000002-1226-4A53-9D24-AFA50C0163A3";
  static constexpr uint32_t bleSolidDurationMs = 5000;
#endif // IOTSA_WITH_BLE
};

#endif
