#include "iotsa.h"
#include "iotsaBLEServer.h"
#include "iotsaConfigFile.h"
#ifdef IOTSA_WITH_BLE
//#include <BLE2902.h>

#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif

// Reboot delay after an isEnabled toggle: longer than the plain HTTP-response
// delay because this reboot exists to re-init (or tear down) the whole NimBLE
// stack. 4s is historical/precautionary, not measured.
static const uint32_t REBOOT_DELAY_BLE_REINIT_MS = 4000;

// How long a client-role connect attempt gets held off (see
// IotsaBLERadioArbiter::reserveConnectionForServer(), iotsaBLE.h) after we last saw
// activity on the server role -- generous enough to cover a full multi-step
// maintenance sequence (enable wifi, request a mode, confirm it, set fields),
// re-armed on every connect/disconnect rather than a single fixed window.
static const uint32_t SERVER_CONNECTION_RESERVE_MS = 30000;

class IotsaBLEServerCallbacks : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    IFBLEDEBUG IotsaSerial.printf("BLE connect\n");
    iotsaController.pauseSleep();
    IotsaBLERadioArbiter::reserveConnectionForServer(SERVER_CONNECTION_RESERVE_MS);
    IotsaBLEServerMod::_notePeerConnected(connInfo.getConnHandle());
  }
	void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    IFBLEDEBUG IotsaSerial.printf("BLE Disconnect reason %d\n", reason);
    iotsaController.resumeSleep();
    IotsaBLEServerMod::_notePeerDisconnected(connInfo.getConnHandle());
    // Re-arm rather than let the reservation end right at disconnect: a
    // maintenance sequence often reconnects a few seconds later for its next
    // step (see docs/device-flashing.md's BLE dances).
    IotsaBLERadioArbiter::reserveConnectionForServer(SERVER_CONNECTION_RESERVE_MS);
    // We're on the NimBLE host task: don't restart advertising here, let
    // loop() decide whether we should be advertising at all (cwi-dis/iotsa#263).
    IotsaBLEServerMod::_requestReconcile();
  }
};

class IotsaBLECharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
  IotsaBLECharacteristicCallbacks(UUIDstring _charUUID, IotsaBLEProvider *_api)
  : charUUID(_charUUID),
    api(_api)
  {}

	void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    IFBLEDEBUG IotsaSerial.printf("BLE char onRead %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaController.noteActivity();
    IotsaBLEServerMod::_notePeerActivity(connInfo.getConnHandle());
    api->bleGetHandler(charUUID);
  }
	void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    IFBLEDEBUG IotsaSerial.printf("BLE char onWrite %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaController.noteActivity();
    IotsaBLEServerMod::_notePeerActivity(connInfo.getConnHandle());
    api->blePutHandler(charUUID);
  }
	void onStatus(NimBLECharacteristic* pCharacteristic, uint32_t code) {
    iotsaController.noteActivity();
    IFBLEDEBUG IotsaSerial.printf("BLE char onStatus\n");
  }
private:
  UUIDstring charUUID;
  IotsaBLEProvider *api;
};

#ifdef IOTSA_WITH_WEB
void
IotsaBLEServerMod::webHandler() {
  bool anyChanged = false;
  if (api.webService->server->hasArg("isEnabled")) {
    bool newIsEnabled = (bool)strtol(api.webService->server->arg("isEnabled").c_str(), 0, 10);
    if (newIsEnabled != isEnabled) {
      isEnabled = newIsEnabled;
      iotsaController.requestReboot(REBOOT_DELAY_BLE_REINIT_MS);
      anyChanged = true;
    }
  }
  if( api.webService->server->hasArg("adv_min")) {
    adv_min = strtol(api.webService->server->arg("adv_min").c_str(), 0, 10);
    anyChanged = true;
  }
  if( api.webService->server->hasArg("adv_max")) {
    adv_max = strtol(api.webService->server->arg("adv_max").c_str(), 0, 10);
    anyChanged = true;
  }if( api.webService->server->hasArg("tx_power_dbm")) {
    tx_power_dbm = strtol(api.webService->server->arg("tx_power_dbm").c_str(), 0, 10);
    anyChanged = true;
  }
  if( api.webService->server->hasArg("idle_timeout")) {
    idle_timeout = strtol(api.webService->server->arg("idle_timeout").c_str(), 0, 10);
    anyChanged = true;
  }
  if (anyChanged) configSave();

  
  String message = "<html><head><title>BLE Server module</title></head><body><h1>BLE Server module</h1>";
  message += "<form method='get'>";
  message += "BLE Enabled: <input type='text' name='isEnabled' value='" + String((int)isEnabled) + "'><br>";
  message += "Advertising interval (min): <input type='text' name='adv_min' value='" + String(adv_min) + "'> (default: -1, unit: 0.625ms, range: 32..16384)<br>";
  message += "Advertising interval (max): <input type='text' name='adv_max' value='" + String(adv_max) + "'> (default: -1, unit: 0.625ms, range: 32..16384)<br>";
  message += "Transmit power level: <input type='text' name='tx_power_dbm' value='" + String(tx_power_dbm) + "'> (raw dBm; -1: leave at hardware default; valid range is chip-dependent, e.g. -12..+9 on classic ESP32, -24..+21 on ESP32-C3/S3/C6)<br>";
  message += "Transmit power level (actual): " + String(tx_power_dbm_actual) + " dBm<br>";
  message += "Idle connection timeout: <input type='text' name='idle_timeout' value='" + String(idle_timeout) + "'> (seconds; disconnect a client that hasn't read or written anything for this long; 0: never)<br>";
  message += "<input type='submit'></form></body></html>";
  api.webService->server->send(200, "text/html", message);
}

String IotsaBLEServerMod::info() {
  String message = "<p>Built with BLE server module";
  if (!isEnabled) message += " (currently disabled)";
  message += ". See <a href=\"/bleserver\">/bleserver</a> to change settings.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

NimBLEServer *IotsaBLEServerMod::s_server = 0;
IotsaBleApiService *IotsaBLEServerMod::s_services = NULL;
int IotsaBLEServerMod::adv_min = -1;
int IotsaBLEServerMod::adv_max = -1;
int IotsaBLEServerMod::tx_power_dbm = -1;
int IotsaBLEServerMod::tx_power_dbm_actual = -1;
volatile uint32_t IotsaBLEServerMod::advertisingRetryAtMillis = 0;
volatile uint32_t IotsaBLEServerMod::advertisingRetryDuration = 0;
volatile bool IotsaBLEServerMod::s_reconcileRequested = false;
bool IotsaBLEServerMod::s_enabled = true;
int IotsaBLEServerMod::idle_timeout = 60;
IotsaBLEServerMod::PeerActivity IotsaBLEServerMod::s_peers[NIMBLE_MAX_CONNECTIONS];

const uint32_t IDLE_CHECK_INTERVAL_MS = 1000;

void IotsaBLEServerMod::_notePeerConnected(uint16_t connHandle) {
  for (auto& p : s_peers) {
    if (p.connHandle == BLE_HS_CONN_HANDLE_NONE) {
      // Timestamp first, so loop() never sees this handle with a stale time.
      p.lastActivityMillis = millis();
      p.connHandle = connHandle;
      return;
    }
  }
  IotsaSerial.printf("IotsaBLEServerMod: no free peer slot for connection %d\n", connHandle);
}

void IotsaBLEServerMod::_notePeerActivity(uint16_t connHandle) {
  for (auto& p : s_peers) {
    if (p.connHandle == connHandle) {
      p.lastActivityMillis = millis();
      return;
    }
  }
}

void IotsaBLEServerMod::_notePeerDisconnected(uint16_t connHandle) {
  for (auto& p : s_peers) {
    if (p.connHandle == connHandle) {
      p.connHandle = BLE_HS_CONN_HANDLE_NONE;
      return;
    }
  }
}

void IotsaBLEServerMod::_checkIdlePeers() {
  if (idle_timeout <= 0 || s_server == nullptr) return;
  uint32_t now = millis();
  if (now - _lastIdleCheckMillis < IDLE_CHECK_INTERVAL_MS) return;
  _lastIdleCheckMillis = now;
  for (auto& p : s_peers) {
    uint16_t h = p.connHandle;
    if (h == BLE_HS_CONN_HANDLE_NONE) continue;
    uint32_t idle = now - p.lastActivityMillis;
    if (idle > (uint32_t)idle_timeout * 1000) {
      // Not debug-gated: a central holding a connection open without using it
      // is exactly the field problem this exists for (cwi-dis/iotsa#265).
      IotsaSerial.printf("IotsaBLEServerMod: connection %d idle for %ds, disconnecting\n", h, (int)(idle / 1000));
      // Reset the timestamp so we don't re-issue every check while the
      // disconnect is in progress; onDisconnect() frees the slot.
      p.lastActivityMillis = now;
      s_server->disconnect(h);
    }
  }
}

const uint32_t ADVERTISING_RETRY_MS = 2000; // How long to wait before retrying a failed advertising start

void IotsaBLEServerMod::_applyTxPower() {
  if (tx_power_dbm != -1) {
    if (!NimBLEDevice::setPower((int8_t)tx_power_dbm)) {
      IotsaSerial.printf("IotsaBLEServerMod: setPower(%d) failed (out of range for this chip?)\n", tx_power_dbm);
    }
  }
  tx_power_dbm_actual = NimBLEDevice::getPower();
}

void IotsaBLEServerMod::_noteAdvertisingStartResult(bool ok, uint32_t duration) {
  if (ok) {
    advertisingRetryAtMillis = 0;
    return;
  }
  IotsaSerial.println("IotsaBLEServerMod: pAdvertising->start() failed (connection pool full?), will retry");
  advertisingRetryDuration = duration;
  advertisingRetryAtMillis = millis() + ADVERTISING_RETRY_MS;
}

void IotsaBLEServerMod::createServer() {
  if (s_server) return;
  iotsaConfig.ensureConfigLoaded();
  IFBLEDEBUG IotsaSerial.print("BLE hostname: ");
  IFBLEDEBUG IotsaSerial.println(iotsaConfig.hostName.c_str());
  iotsaBLE_ensureInitialized();
  NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);
  _applyTxPower();
  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(new IotsaBLEServerCallbacks());
  IotsaBLERadioArbiter::setAdvertisingReconciler(_reconcileAdvertising);
  // NimBLE-Arduino 2.1.0 stopped advertising the device name by default, and
  // scan response is no longer enabled by default either. Turn scan response
  // back on and set the name explicitly (setName() puts it in the scan
  // response payload, since scan response is enabled) so active scanners
  // (which iotsaBLEClient always is) get it back via the standard
  // advertisement + scan-response mechanism.
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->enableScanResponse(true);
  pAdvertising->setName(iotsaConfig.hostName.c_str());
}

void IotsaBLEServerMod::_reconcileAdvertising(uint32_t durationMs) {
  if (!s_enabled || s_server == nullptr) return;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising == nullptr) return;
  bool want = iotsaController.bleRadioWanted() && IotsaBLERadioArbiter::advertisingPauseReasons() == 0;
  bool active = pAdvertising->isAdvertising();
  if (!want) {
    advertisingRetryAtMillis = 0; // a pending retry must not undo this
    if (active) {
      IFBLEDEBUG IotsaSerial.printf("BLE stop advertising (wanted=%d, pause reasons=0x%x)\n",
        (int)iotsaController.bleRadioWanted(), IotsaBLERadioArbiter::advertisingPauseReasons());
      pAdvertising->stop();
      iotsaBLE_notifyAdvertisingStateChanged(false);
    }
    return;
  }
  if (active) return;
  const uint32_t requestedMs = durationMs; // what a retry should ask for again
  // A limited-duration start (the light-sleep wake window) needs at least one
  // full advertising cycle to have a chance of being seen -- it must scale
  // with adv_min (raw units, 0.625ms each; BLE_HCI_ADV_ITVL_MIN=32 units=20ms
  // is the legal minimum, confirmed against NimBLE source), not stay fixed, or
  // it silently goes stale for any server configured slower than the legal
  // minimum (e.g. control's adv_min=100). The +50ms margin is calibrated, not
  // derived from first principles: chosen so this reproduces the previous
  // hardcoded, already field-tested value (70ms) at the legal-minimum
  // interval. A window too short for that advertises indefinitely instead.
  const int advIntervalMillis = (adv_min >= 0 ? adv_min : 32) * 5 / 8;
  const uint32_t minAdvertisingDurationMillis = advIntervalMillis + 50;
  // Independent of advertise interval -- this is BLE connection-establishment
  // handshake time, not an advertising-cycle cost.
  const uint32_t extraDurationForConnectingMillis = 30;
  if (durationMs != 0 && durationMs < minAdvertisingDurationMillis + extraDurationForConnectingMillis) {
    durationMs = 0;
  } else if (durationMs != 0) {
    durationMs -= extraDurationForConnectingMillis;
  }
  bool ok;
  if (durationMs == 0) {
    IFBLEDEBUG IotsaSerial.println("BLE start advertising");
    ok = pAdvertising->start();
  } else {
    IFBLEDEBUG IotsaSerial.printf("BLE start advertising for %u ms\n", (unsigned)durationMs);
    ok = pAdvertising->start(durationMs);
  }
  iotsaBLE_notifyAdvertisingStateChanged(ok);
  _noteAdvertisingStartResult(ok, requestedMs);
}

void IotsaBLEServerMod::setup() {
  for (auto& p : s_peers) p.connHandle = BLE_HS_CONN_HANDLE_NONE;
  createServer();
  configLoad();   // sets isEnabled from bleserver.cfg (default true)
  s_enabled = isEnabled;
  if (!isEnabled) {
    IFBLEDEBUG IotsaSerial.println("BLE deinit, not isEnabled");
    NimBLEDevice::deinit(false);
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    return;
  }
}

bool IotsaBLEServerMod::getHandler(const char *path, JsonObject& reply) {
  reply["isEnabled"] = isEnabled;
  reply["adv_min"] = adv_min;
  reply["adv_max"] = adv_max;
  reply["tx_power_dbm"] = tx_power_dbm;
  reply["tx_power_dbm_actual"] = tx_power_dbm_actual;
  reply["idle_timeout"] = idle_timeout;
  // Read-only state, so a stuck connection (cwi-dis/iotsa#265) is visible over WiFi.
  reply["connected_peers"] = s_server ? (int)s_server->getConnectedCount() : 0;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::isInitialized() ? NimBLEDevice::getAdvertising() : nullptr;
  reply["advertising"] = pAdvertising != nullptr && pAdvertising->isAdvertising();
  // Why not, when it isn't (cwi-dis/iotsa#263): IotsaBLERadioArbiter pause
  // reasons, 1=sleep 2=scan 4=GATT build. 0 with advertising false means the
  // policy (bleRadioWanted) or a failed start.
  reply["advertising_paused"] = IotsaBLERadioArbiter::advertisingPauseReasons();
  return true;
}

bool IotsaBLEServerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonObject reqObj = request.as<JsonObject>();
  bool anyChanged = false;
  bool newEnabled = isEnabled;
  if (getFromRequest<int>(reqObj, "isEnabled", newEnabled) && newEnabled != isEnabled) {
    anyChanged = true;
    isEnabled = request["isEnabled"];
    iotsaController.requestReboot(REBOOT_DELAY_BLE_REINIT_MS);
  }
  if (getFromRequest<int>(reqObj, "adv_min", adv_min)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "adv_max", adv_max)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "tx_power_dbm", tx_power_dbm)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "idle_timeout", idle_timeout)) anyChanged = true;
  if (anyChanged) configSave();
  checkUnhandled(reqObj);
  return anyChanged;
}

void IotsaBLEServerMod::lateSetup() {
  api.setup("bleserver", true, true);
  name = "bleserver";
}

void IotsaBLEServerMod::lateSetupDone() {
  // All services are built now. Services no longer need starting explicitly:
  // NimBLEService::start() is a deprecated no-op, NimBLEAdvertising::start()
  // starts the GATT server itself. Whether we actually advertise is up to
  // _reconcileAdvertising() -- the boot enable/disable decision is
  // IotsaController policy (cwi-dis/iotsa#106), seeded from !bleDisabledOnBoot.
  _lastBleRadioWanted = iotsaController.bleRadioWanted();
  IotsaBLERadioArbiter::resumeAdvertising(IotsaBLERadioArbiter::PAUSE_GATT_BUILD);
}

void IotsaBLEServerMod::configLoad() {
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  IotsaConfigFileLoad cf("/config/bleserver.cfg");
  cf.get("isEnabled", isEnabled, true);
  cf.get("adv_min", adv_min, adv_min);
  if (adv_min >= 0) pAdvertising->setMinInterval(adv_min);
  cf.get("adv_max", adv_max, adv_max);
  if (adv_max >= 0) pAdvertising->setMaxInterval(adv_max);
  cf.get("tx_power_dbm", tx_power_dbm, tx_power_dbm);
  cf.get("idle_timeout", idle_timeout, idle_timeout);
  _applyTxPower();
#ifdef IOTSA_BLE_DEBUG
  pAdvertising->setAdvertisingCompleteCallback([](NimBLEAdvertising* adv) {
    IFBLEDEBUG IotsaSerial.println("BLE advertising complete callback");
  });
#endif
}

void IotsaBLEServerMod::configSave() {
  IotsaConfigFileSave cf("/config/bleserver.cfg");
  cf.put("isEnabled", isEnabled);
  cf.put("adv_min", adv_min);
  cf.put("adv_max", adv_max);
  cf.put("tx_power_dbm", tx_power_dbm);
  cf.put("idle_timeout", idle_timeout);
  if (NimBLEDevice::isInitialized()) {
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    // Stop so the new interval/power take effect, then let the reconciler
    // decide whether to start again (it used to restart unconditionally,
    // even while paused or with BLE off by policy).
    pAdvertising->stop();
    if (adv_min >= 0) pAdvertising->setMinInterval(adv_min);
    if (adv_max >= 0) pAdvertising->setMaxInterval(adv_max);
    _applyTxPower();
    _reconcileAdvertising();
  }
}

void IotsaBLEServerMod::loop() {
  // Reconcile advertising on events only (see _reconcileAdvertising()): a
  // change in IotsaController's BLE radio policy (cwi-dis/iotsa#106), a request
  // from the NimBLE host task (a peer disconnected), or a due retry.
  bool bleWanted = iotsaController.bleRadioWanted();
  if (bleWanted != _lastBleRadioWanted) {
    IFBLEDEBUG IotsaSerial.printf("BLE radio %s by policy\n", bleWanted ? "wanted" : "not wanted");
    _lastBleRadioWanted = bleWanted;
    _reconcileAdvertising();
  }
  if (s_reconcileRequested) {
    s_reconcileRequested = false;
    _reconcileAdvertising();
  }
  if (advertisingRetryAtMillis != 0 && millis() >= advertisingRetryAtMillis) {
    advertisingRetryAtMillis = 0;
    IFBLEDEBUG IotsaSerial.println("BLE retry start advertising");
    _reconcileAdvertising(advertisingRetryDuration);
  }
  _checkIdlePeers();
}

void IotsaBleApiService::setup(const char* serviceUUID, IotsaBLEProvider *_apiProvider) {
  // No advertising while the GATT table is being built. The pause is lifted in
  // IotsaBLEServerMod::lateSetupDone(), once every service exists. xxxjack:
  // resuming it right here instead has proved wrong.
  IotsaBLERadioArbiter::pauseAdvertising(IotsaBLERadioArbiter::PAUSE_GATT_BUILD);
  IotsaBLEServerMod::createServer();
  next = IotsaBLEServerMod::s_services;
  IotsaBLEServerMod::s_services = this;
  apiProvider = _apiProvider;
  IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: create ble service %s to 0x%x\n", serviceUUID, (uint32_t)apiProvider);
  bleService = IotsaBLEServerMod::s_server->createService(serviceUUID);

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
}

void IotsaBleApiService::addCharacteristic(UUIDstring charUUID, int mask, uint8_t d2904format, uint16_t d2904unit, const char *d2901descr) {
  IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: add ble characteristic %s mask %d\n", charUUID, mask);
  nCharacteristic++;
  characteristicUUIDs = (UUIDstring *)realloc((void *)characteristicUUIDs, nCharacteristic*sizeof(UUIDstring));
  bleCharacteristics = (NimBLECharacteristic **)realloc((void *)bleCharacteristics, nCharacteristic*sizeof(NimBLECharacteristic *));
  if (characteristicUUIDs == NULL || bleCharacteristics == NULL) {
    IotsaSerial.println("IotsaBleApiService: addCharacteristic out of memory");
    return;
  }
  NimBLECharacteristic *newChar = bleService->createCharacteristic(charUUID, mask);
  newChar->setCallbacks(new IotsaBLECharacteristicCallbacks(charUUID, apiProvider));
  NimBLEDescriptor *d2901 = newChar->createDescriptor("2901");
  NimBLE2904 *d2904 = newChar->create2904();
  d2901->setValue(std::string(d2901descr));
  d2904->setFormat(d2904format);
  d2904->setUnit(d2904unit);

  characteristicUUIDs[nCharacteristic-1] = charUUID;
  bleCharacteristics[nCharacteristic-1] = newChar;
}

void IotsaBleApiService::set(UUIDstring charUUID, const uint8_t *data, size_t size) {
  IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: set(%s) len=%d\n", charUUID, size);
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      NimBLECharacteristic* ch = bleCharacteristics[i];
      ch->setValue(NULL, 0);
      ch->setValue((uint8_t *)data, size);
      if (ch->getLength() != size) {
        IotsaSerial.printf("IotsaBleApiService: set: size=%d expected %d\n", ch->getLength(), size);
      }
      bool want_notify = ch->getProperties() & NIMBLE_PROPERTY::NOTIFY;
      bool want_indicate = ch->getProperties() & NIMBLE_PROPERTY::INDICATE;
      if(want_notify) {
        IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: send notify %s len=%d\n", charUUID, size);
        ch->notify((uint8_t *)data, size);
      }
      if (want_indicate) {
        IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: send indicate %s len=%d\n", charUUID, size);
        ch->indicate((uint8_t *)data, size);
      }
      return;
    }
  }
  IotsaSerial.printf("IotsaBleApiService: set: unknown characteristic %s\n", charUUID);
}

void IotsaBleApiService::set(UUIDstring charUUID, uint8_t value) {
  set(charUUID, &value, 1);
}

void IotsaBleApiService::set(UUIDstring charUUID, uint16_t value) {
  set(charUUID, (const uint8_t *)&value, 2);
}

void IotsaBleApiService::set(UUIDstring charUUID, uint32_t value) {
  set(charUUID, (const uint8_t *)&value, 4);
}

void IotsaBleApiService::set(UUIDstring charUUID, const std::string& value) {
  set(charUUID, (const uint8_t *)value.c_str(), value.length());
}

void IotsaBleApiService::set(UUIDstring charUUID, const String& value) {
  set(charUUID, (const uint8_t *)value.c_str(), value.length());
}


int IotsaBleApiService::getAsInt(UUIDstring charUUID) {
  int val = 0;
  int shift = 0;
  std::string buffer = getAsString(charUUID);
  for(auto ptr = buffer.begin(); ptr != buffer.end(); ptr++) {
    int byte = (uint8_t)*ptr;
    val = val | (byte << shift);
    shift += 8;
  }
  return val;
}

std::string IotsaBleApiService::getAsString(UUIDstring charUUID) {
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      auto value = bleCharacteristics[i]->getValue();
      return std::string(value.c_str(), value.size());
    }
  }
  IotsaSerial.println("IotsaBleApiService: get: unknown characteristic");
  return "";
}


#endif // IOTSA_WITH_BLE