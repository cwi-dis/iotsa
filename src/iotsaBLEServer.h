#ifndef _IOTSABLESERVER_H_
#define _IOTSABLESERVER_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_BLE
#ifdef IOTSA_WITH_NIMBLE
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2904.h>
#endif

#ifdef IOTSA_WITH_API
#define IotsaBLEServerModBaseMod IotsaApiMod
#else
#define IotsaBLEServerModBaseMod IotsaMod
#endif
typedef const char * UUIDstring;

class IotsaBLEServerMod;
class IotsaBLEApiProvider;

#ifdef IOTSA_WITH_NIMBLE
class BLE2901  {
public:
  BLE2901(const char *description) {}
};

#else
class BLE2901 : public BLEDescriptor {
public:
  BLE2901(const char *description) : BLEDescriptor(BLEUUID((uint16_t)0x2901)) { setValue((uint8_t *)description, strlen(description)); }
};
#endif

class IotsaBLEApiProvider {
public:
  typedef const char * UUIDstring;

  virtual ~IotsaBLEApiProvider() {}
  virtual bool blePutHandler(UUIDstring charUUID) = 0;
  virtual bool bleGetHandler(UUIDstring charUUID) = 0;

  static const uint32_t BLE_READ = NIMBLE_PROPERTY::READ;
  static const uint32_t BLE_WRITE = NIMBLE_PROPERTY::WRITE;
  static const uint32_t BLE_NOTIFY = NIMBLE_PROPERTY::NOTIFY;
};

class IotsaBleApiService {
  friend class IotsaBLEServerMod;
public:
  typedef IotsaBLEApiProvider::UUIDstring UUIDstring;
  IotsaBleApiService(IotsaBLEServerMod *_mod=NULL)
  : apiProvider(NULL),
    bleService(NULL),
    nCharacteristic(0),
    characteristicUUIDs(NULL),
    bleCharacteristics(NULL)
  {}
  void setup(const char* serviceUUID, IotsaBLEApiProvider *_apiProvider);
  void addCharacteristic(UUIDstring charUUID, int mask, BLEDescriptor *d1 = NULL, BLEDescriptor *d2 = NULL, BLEDescriptor *d3 = NULL);
  void addCharacteristic(UUIDstring charUUID, int mask, uint8_t d2904format, uint16_t d2904unit, const char *d2901 = NULL);
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
  IotsaBleApiService *next;
};

class IotsaBLEServerMod : public IotsaBLEServerModBaseMod {
  friend class IotsaBleApiService;
public:
  using IotsaBLEServerModBaseMod::IotsaBLEServerModBaseMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
  static void setAdvertisingInterval(uint16_t _adv_min, uint16_t _adv_max) {
    adv_min = _adv_min;
    adv_max = _adv_max;
  }

  static bool pauseServer();
  static void resumeServer();
protected:
  bool isEnabled;
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
  void handler();

  static void createServer();
  static void startServer();
  static BLEServer *s_server;
  static IotsaBleApiService *s_services;

  static uint16_t adv_min;
  static uint16_t adv_max;
};
#else // IOTSA_WITH_BLE
class IotsaBLEApiProvider {};
#endif
#endif
