#ifndef _IOTSABLESERVER_H_
#define _IOTSABLESERVER_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#ifdef IOTSA_WITH_API
#define IotsaBLEServerModBaseMod IotsaApiMod
#else
#define IotsaBLEServerModBaseMod IotsaMod
#endif
typedef const char * UUIDstring;

class IotsaBLEServerMod;
class IotsaBLEApiProvider;


class IotsaBLEApiProvider {
public:
  typedef const char * UUIDstring;

  virtual ~IotsaBLEApiProvider() {}
  virtual bool blePutHandler(UUIDstring charUUID) = 0;
  virtual bool bleGetHandler(UUIDstring charUUID) = 0;

  static const uint32_t BLE_READ = BLECharacteristic::PROPERTY_READ;
  static const uint32_t BLE_WRITE = BLECharacteristic::PROPERTY_WRITE;
  static const uint32_t BLE_NOTIFY = BLECharacteristic::PROPERTY_NOTIFY;
};

class IotsaBleApiService {
  typedef IotsaBLEApiProvider::UUIDstring UUIDstring;
public:
  IotsaBleApiService(IotsaBLEServerMod *_mod=NULL)
  : apiProvider(NULL),
    bleService(NULL),
    nCharacteristic(0),
    characteristicUUIDs(NULL),
    bleCharacteristics(NULL)
  {}
  void setup(const char* serviceUUID, IotsaBLEApiProvider *_apiProvider);
  void addCharacteristic(UUIDstring charUUID, int mask);
  void set(UUIDstring charUUID, const uint8_t *data, size_t size);
  void set(UUIDstring charUUID, uint8_t value);
  void set(UUIDstring charUUID, uint16_t value);
  void set(UUIDstring charUUID, uint32_t value);
  void set(UUIDstring charUUID, const std::string& value);
  void set(UUIDstring charUUID, const String& value);
  void getAsBuffer(UUIDstring charUUID, uint8_t **datap, size_t *sizep);
  int getAsInt(UUIDstring charUUID);
  std::string getAsString(UUIDstring charUUID);
protected:
  IotsaBLEApiProvider *apiProvider;
  BLEService *bleService;
  int nCharacteristic;
  UUIDstring  *characteristicUUIDs;
  BLECharacteristic **bleCharacteristics;
};

class IotsaBLEServerMod : public IotsaBLEServerModBaseMod {
  friend class IotsaBleApiService;
public:
  using IotsaBLEServerModBaseMod::IotsaBLEServerModBaseMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
  static void setAdvertisingInterval(uint16_t _adv_min, uint16_t _adv_max) {
    adv_min = _adv_min;
    adv_max = _adv_max;
  }

protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void configLoad();
  void configSave();
  void handler();

  static void createServer();
  static BLEServer *s_server;

  static uint16_t adv_min;
  static uint16_t adv_max;
};
#else // IOTSA_WITH_BLE
class IotsaBLEApiProvider {};
#endif
#endif