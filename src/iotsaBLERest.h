#ifndef _IOTSABLEREST_H_
#define _IOTSABLEREST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLEServer.h"

#ifdef IOTSA_WITH_BLEREST
#ifndef IOTSA_WITH_API
#error IOTSA_WITH_BLEREST requires IOTSA_WITH_API
#endif
#ifndef IOTSA_WITH_BLE
#error IOTSA_WITH_BLEREST requires IOTSA_WITH_BLE
#endif
#define IotsaBLERestModBaseMod IotsaApiMod

class IotsaBLERestMod : public IotsaBLERestModBaseMod, public IotsaBLEApiProvider {
public:
  using IotsaBLERestModBaseMod::IotsaBLERestModBaseMod;
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
  static constexpr UUIDstring serviceUUID = "E2DB0001-D1D6-4564-961F-4B7E8B15ADE6";
  static constexpr UUIDstring commandUUID = "E2DB0002-D1D6-4564-961F-4B7E8B15ADE6";
  static constexpr UUIDstring dataUUID = "E2DB0003-D1D6-4564-961F-4B7E8B15ADE6";
  static constexpr UUIDstring responseUUID = "E2DB0004-D1D6-4564-961F-4B7E8B15ADE6";

  std::string curCommand;
  std::string curData;
  std::string curResponse;
  int _processRequest();
};

#endif
#endif
