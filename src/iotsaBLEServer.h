#ifndef _IOTSABLESERVER_H_
#define _IOTSABLESERVER_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaBLE.h"

#ifdef IOTSA_WITH_BLE

class IotsaBLEServerMod;

class BLE2901  {
public:
  BLE2901(const char *description) {}
};

class IotsaBleApiService {
  friend class IotsaBLEServerMod;
public:
  typedef IotsaBLEProvider::UUIDstring UUIDstring;
  // Characteristic property flags for addCharacteristic()'s mask parameter --
  // live here (not on IotsaBLEProvider) because they need NIMBLE_PROPERTY,
  // and IotsaBLEProvider is core/unconditional (see cwi-dis/iotsa#206) while
  // this service class is already BLE-specific.
  static const uint32_t BLE_READ = NIMBLE_PROPERTY::READ;
  static const uint32_t BLE_WRITE = NIMBLE_PROPERTY::WRITE;
  static const uint32_t BLE_NOTIFY = NIMBLE_PROPERTY::NOTIFY;
  IotsaBleApiService(IotsaBLEServerMod *_mod=NULL)
  : apiProvider(NULL),
    bleService(NULL),
    nCharacteristic(0),
    characteristicUUIDs(NULL),
    bleCharacteristics(NULL)
  {}
  // advertise: also list this service's UUID in the advertisement/scan
  // response (cwi-dis/iotsa#277), so scanners can recognise the device
  // without connecting. Every service is in the GATT database either way;
  // after connecting a client finds all of them through service discovery.
  // There's room for the core runmode service plus one 128-bit service of
  // the app's own (in the scan response, if the device name is at most 11
  // characters) and a few 16-bit ones, so advertise only an app's main
  // service. Whatever doesn't fit is logged, not silently dropped.
  void setup(const char* serviceUUID, IotsaBLEProvider *_apiProvider, bool advertise=true);
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
  IotsaBLEProvider *apiProvider;
  NimBLEService *bleService;
  int nCharacteristic;
  UUIDstring  *characteristicUUIDs;
  NimBLECharacteristic **bleCharacteristics;
  IotsaBleApiService *next;
};

class IotsaBLEServerMod : public IotsaModule, public IotsaSingletonModule<IotsaBLEServerMod> {
  friend class IotsaBleApiService;
public:
  // See IotsaBaseModule: deleted so old code passing an auth provider fails to compile.
  IotsaBLEServerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool _early=false) = delete;
  IotsaBLEServerMod(IotsaApplication &_app, bool _early=false)
  : IotsaModule(_app, _early)
  {
    claimSingleton(this);
  }
  void setup() override;
  void lateSetup() override;
  void lateSetupDone() override;
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

  // The single place that starts or stops advertising (cwi-dis/iotsa#263):
  // advertise iff BLE is enabled, iotsaController.bleRadioWanted(), and no
  // IotsaBLERadioArbiter pause reason is set. Installed as the arbiter's
  // reconciler, so pause/resume calls land here; also called from loop() on a
  // policy change, a due retry, or a request from the NimBLE host task (see
  // _requestReconcile()). Event-driven on purpose, not run every loop(): a
  // light-sleep wake window starts advertising with a duration, and that must
  // be allowed to run out without being restarted. durationMs: see
  // IotsaBLERadioArbiter::resumeAdvertising(). Main task only.
  static void _reconcileAdvertising(uint32_t durationMs = 0);
  // For callers on the NimBLE host task (onDisconnect()): just sets a flag,
  // loop() does the reconcile.
  static void _requestReconcile() { s_reconcileRequested = true; }
  // pAdvertising->start() can fail -- e.g. BLE_HS_ENOMEM when the shared
  // connection pool is exhausted by outbound client connections. On failure
  // this arms a retry that loop() acts on (through _reconcileAdvertising(),
  // so a retry never starts advertising that has meanwhile been paused).
  static void _noteAdvertisingStartResult(bool ok, uint32_t duration);
  // Per-connection activity tracking for the idle-connection timeout
  // (cwi-dis/iotsa#265): a central that stays alive but never closes its
  // connection would otherwise keep us from advertising indefinitely. Called
  // from the NimBLE host task; only writes plain volatile scalars, loop()
  // does the actual disconnecting.
  static void _notePeerConnected(uint16_t connHandle);
  static void _notePeerActivity(uint16_t connHandle);
  static void _notePeerDisconnected(uint16_t connHandle);
protected:
  bool isEnabled = true;   // config.cfg overrides in configLoad()
  // Last iotsaController.bleRadioWanted() applied by loop(), so a policy change
  // is acted on once (cwi-dis/iotsa#106). Starts true; lateSetupDone() resyncs it.
  bool _lastBleRadioWanted = true;
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif

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
  // failure, cleared by it on success and whenever _reconcileAdvertising()
  // decides advertising should be off. loop() is the only reader.
  static volatile uint32_t advertisingRetryAtMillis;
  // Duration to retry with (0 = indefinite) -- whatever the failed call used.
  static volatile uint32_t advertisingRetryDuration;
  // Set by _requestReconcile(), consumed by loop().
  static volatile bool s_reconcileRequested;
  // Static mirror of isEnabled, for the static _reconcileAdvertising(): false
  // means the stack was deinit()ed in setup(), so never touch advertising.
  static bool s_enabled;
  // Disconnect a peer that hasn't read or written anything for this many
  // seconds. 0: never. See _notePeerConnected() above.
  static int idle_timeout;
  struct PeerActivity {
    volatile uint16_t connHandle;   // BLE_HS_CONN_HANDLE_NONE: slot free
    volatile uint32_t lastActivityMillis;
  };
  static PeerActivity s_peers[NIMBLE_MAX_CONNECTIONS];
  uint32_t _lastIdleCheckMillis = 0;
  void _checkIdlePeers();
private:
  // Applies tx_power_dbm via NimBLEDevice::setPower() (unless it's -1), then
  // sets tx_power_dbm_actual to the level read back via getPower() -- see
  // the field comments above.
  static void _applyTxPower();
};
#endif // IOTSA_WITH_BLE
#endif
