#ifndef _IOTSAHPS_H_
#define _IOTSAHPS_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

#ifdef IOTSA_WITH_HPS
#ifndef IOTSA_WITH_API
#error IOTSA_WITH_HPS requires IOTSA_WITH_API
#endif
#ifndef IOTSA_WITH_BLE
#error IOTSA_WITH_HPS requires IOTSA_WITH_BLE
#endif
#define IotsaHpsModBaseMod IotsaApiMod

class IotsaHpsMod : public IotsaHpsModBaseMod, public IotsaBLEApiProvider {
  const int HPSMaxBodySize = 512;
  enum HPSControl {
    NONE=0,
    GET=0x01,
    POST=0x03,
    PUT=0x04
  };
  enum HPSDataStatus {
    EMPTY=0,
    HeadersReceived = 0x01,
    HeadersTruncated = 0x02,
    BodyReceived = 0x04,
    BodyTruncated = 0x08
  };
public:
  using IotsaHpsModBaseMod::IotsaHpsModBaseMod;
  void setup() override;
  void loop() override;
  void serverSetup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;

  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "1823";
  static constexpr UUIDstring urlUUID = "2AB6";
  static constexpr UUIDstring headersUUID = "2AB7";
  static constexpr UUIDstring statusUUID = "2AB8";
  static constexpr UUIDstring bodyUUID = "2AB9";
  static constexpr UUIDstring controlPointUUID = "2ABA";
  static constexpr UUIDstring securityUUID = "2ABB";

  std::string curUrl;
  std::string curHeaders;
  std::string curBody;
  HPSControl curControl;
  uint16_t curHttpStatus;
  HPSDataStatus curDataStatus;
  int _processRequest(HPSControl command);
};

#endif // IOTSA_WITH_HPS
#endif
