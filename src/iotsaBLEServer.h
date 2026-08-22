#ifndef _IOTSABLESERVER_H_
#define _IOTSABLESERVER_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBle.h"

#ifdef IOTSA_WITH_BLE

#ifdef IOTSA_WITH_API
#define IotsaBLEServerModBaseMod IotsaApiMod
#else
#define IotsaBLEServerModBaseMod IotsaMod
#endif

class IotsaBLEServerMod;
class IotsaBLEApiProvider;

class BLE2901  {
public:
  BLE2901(const char *description) {}
};

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
  void addCharacteristic(UUIDstring charUUID, int mask, uint8_t d2904format, uint16_t d2904unit, const char *d2901 = NULL);
  void set(UUIDstring charUUID, const uint8_t *data, size_t size);
  void set(UUIDstring charUUID, uint8_t value);
  void set(UUIDstring charUUID, uint16_t value);
  void set(UUIDstring charUUID, uint32_t value);
  void set(UUIDstring charUUID, const std::string& value);
  void set(UUIDstring charUUID, const String& value);
  //void getAsBuffer(UUIDstring charUUID, uint8_t **datap, size_t *sizep);
  int getAsInt(UUIDstring charUUID);
  std::string getAsString(UUIDstring charUUID);
protected:
  IotsaBLEApiProvider *apiProvider;
  NimBLEService *bleService;
  int nCharacteristic;
  UUIDstring  *characteristicUUIDs;
  NimBLECharacteristic **bleCharacteristics;
  IotsaBleApiService *next;
};

class IotsaBLEServerMod : public IotsaBLEServerModBaseMod {
  friend class IotsaBleApiService;
public:
  using IotsaBLEServerModBaseMod::IotsaBLEServerModBaseMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
#if 0
  static void setAdvertisingInterval(uint16_t _adv_min, uint16_t _adv_max) {
    adv_min = _adv_min;
    adv_max = _adv_max;
  }
#endif

  static bool pauseServer();
  static void resumeServer(int duration=0); // duration=0 means advertise indefinitely
  // pAdvertising->start() (and NimBLEServer::startAdvertising(), used by
  // IotsaBLEServerCallbacks::onDisconnect()) can fail -- e.g. BLE_HS_ENOMEM
  // when the shared connection pool is exhausted by outbound client
  // connections -- and every call site used to silently discard that,
  // leaving the device permanently non-advertising with no log output and
  // no retry. Every start() call site now reports its outcome here instead;
  // on failure this arms a retry that loop() acts on. Safe to call from any
  // task (e.g. the NimBLE host task, which is where onDisconnect() runs) --
  // only writes plain volatile scalars, the actual retried start() call
  // always happens from loop().
  static void _noteAdvertisingStartResult(bool ok, uint32_t duration);
protected:
  bool isEnabled;
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
  void handler();

  static void createServer();
  static NimBLEServer *s_server;
  static IotsaBleApiService *s_services;

  static int adv_min;  // Minimum advertising interval (-1: default)
  static int adv_max;  // Maximum advertising interval (-1: default)
  // Requested transmit power in raw dBm, passed to NimBLEDevice::setPower().
  // -1: don't request a level, use whatever NimBLE/hardware defaults to.
  // User/REST/web-settable, persisted verbatim -- _applyTxPower() never
  // modifies this, so the -1 sentinel survives indefinitely (unlike
  // tx_power_dbm_actual below).
  static int tx_power_dbm;
  // Actual transmit power in raw dBm, as read back via NimBLEDevice::getPower()
  // after every _applyTxPower() call (valid dBm ranges differ per ESP32
  // variant, so this is what's really in effect, not just what was asked
  // for). Read-only -- not settable via REST/web, not persisted to flash.
  static int tx_power_dbm_actual;
  // 0 means no retry pending. Set by _noteAdvertisingStartResult() on
  // failure, cleared by it on success and by any intentional stop/pause
  // (so a retry never fights an explicit pause). loop() is the only reader.
  static volatile uint32_t advertisingRetryAtMillis;
  // Duration to retry with (0 = indefinite) -- whatever the failed call used.
  static volatile uint32_t advertisingRetryDuration;
private:
  void _startServer();
  static void _bleGotoMode();
  // Applies tx_power_dbm via NimBLEDevice::setPower() (unless it's -1), then
  // sets tx_power_dbm_actual to the level read back via getPower() -- see
  // the field comments above.
  static void _applyTxPower();
};
#else // IOTSA_WITH_BLE
class IotsaBLEApiProvider {};
#endif
#endif
