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

class IotsaBLEServerCallbacks : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    IFBLEDEBUG IotsaSerial.printf("BLE connect\n");
    iotsaConfig.pauseSleep();
  }
	void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    IFBLEDEBUG IotsaSerial.printf("BLE Disconnect reason %d, restart advertising\n", reason);
    iotsaConfig.resumeSleep();
    bool ok = pServer->startAdvertising();
    IotsaBLEServerMod::_noteAdvertisingStartResult(ok, 0);
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
    iotsaConfig.postponeSleep(0);
    api->bleGetHandler(charUUID);
  }
	void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    IFBLEDEBUG IotsaSerial.printf("BLE char onWrite %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaConfig.postponeSleep(0);
    api->blePutHandler(charUUID);
  }
	void onStatus(NimBLECharacteristic* pCharacteristic, uint32_t code) {
    iotsaConfig.postponeSleep(0);
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
  if (server->hasArg("isEnabled")) {
    bool newIsEnabled = (bool)strtol(server->arg("isEnabled").c_str(), 0, 10);
    if (newIsEnabled != isEnabled) {
      isEnabled = newIsEnabled;
      iotsaConfig.requestReboot(4000);
      anyChanged = true;
    }
  }
  if( server->hasArg("adv_min")) {
    adv_min = strtol(server->arg("adv_min").c_str(), 0, 10);
    anyChanged = true;
  }
  if( server->hasArg("adv_max")) {
    adv_max = strtol(server->arg("adv_max").c_str(), 0, 10);
    anyChanged = true;
  }if( server->hasArg("tx_power_dbm")) {
    tx_power_dbm = strtol(server->arg("tx_power_dbm").c_str(), 0, 10);
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
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
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

void IotsaBLEServerMod::_startServer() {
  // Note: services no longer need starting explicitly here -- NimBLEService::start()
  // is now a deprecated no-op; NimBLEAdvertising::start() (called via _bleGotoMode()
  // below) starts the GATT server itself before advertising begins.
  if (iotsaConfig.bleDisabledOnBoot) {
    iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_DISABLED;
  } else {
    iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_ENABLED;
  }
  _bleGotoMode();
}

void IotsaBLEServerMod::_bleGotoMode() {
  // if (s_server == nullptr) return;
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  if (pAdvertising == nullptr) return;
  bool wasActive = pAdvertising->isAdvertising();
  bool isActive = iotsaConfig.bleMode == iotsa_ble_mode::IOTSA_BLE_ENABLED;
  if (wasActive == isActive) {
    IFBLEDEBUG IotsaSerial.printf("BLE advertising is already %s\n", isActive ? "active" : "inactive");
    return;
  }
  if (isActive) {
    IFBLEDEBUG IotsaSerial.println("BLE start advertising");
    // causes crash: esp_bt_controller_enable(esp_bt_mode_t::ESP_BT_MODE_BLE);
    bool ok = pAdvertising->start();
    iotsaBLE_notifyAdvertisingStateChanged(ok);
    _noteAdvertisingStartResult(ok, 0);
  } else {
    IFBLEDEBUG IotsaSerial.println("BLE stop advertising");
    advertisingRetryAtMillis = 0; // explicit stop takes priority over any pending retry
    pAdvertising->stop();
    iotsaBLE_notifyAdvertisingStateChanged(false);
    // re-enabling causes crash: esp_bt_controller_disable();
  }
}

bool IotsaBLEServerMod::pauseServer() {
  // For now we keep pauseServer() and resumeServer(), because the use case is for light sleep.
  // An explicit pause always takes priority over any pending advertising-start
  // retry -- clear it unconditionally, even if we're already not advertising
  // (a retry could otherwise still be pending and fire later, fighting this
  // pause).
  advertisingRetryAtMillis = 0;
  if (s_server) {
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising == nullptr || !pAdvertising->isAdvertising()) return true;
    IFBLEDEBUG IotsaSerial.println("BLE pause advertising");
    pAdvertising->stop();
    iotsaBLE_notifyAdvertisingStateChanged(false);
    return true;
  }
  return false;
}

void IotsaBLEServerMod::resumeServer(int duration) {
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  // minAdvertisingDurationMillis needs at least one full advertising cycle to
  // have a chance of being seen -- it must scale with adv_min (raw units,
  // 0.625ms each; BLE_HCI_ADV_ITVL_MIN=32 units=20ms is the legal minimum,
  // confirmed against NimBLE source), not stay fixed, or it silently goes
  // stale for any server configured slower than the legal minimum (e.g.
  // control's adv_min=100). The +50ms margin is calibrated, not derived from
  // first principles: chosen so this reproduces the previous hardcoded,
  // already field-tested value (70ms) at the legal-minimum interval, rather
  // than guessing at a BLE-timing formula the original 70ms comment's own
  // arithmetic didn't fully reconcile (it claimed "20ms cycle + 10ms margin
  // = 70ms", which doesn't add up).
  const int advIntervalMillis = (adv_min >= 0 ? adv_min : 32) * 5 / 8;
  const int minAdvertisingDurationMillis = advIntervalMillis + 50;
  // Independent of advertise interval -- this is BLE connection-establishment
  // handshake time, not an advertising-cycle cost.
  const int extraDurationForConnectingMillis = 30;
  if (duration == 0 || duration < minAdvertisingDurationMillis + extraDurationForConnectingMillis) {
    IFBLEDEBUG IotsaSerial.println("BLE resume advertising");
    bool ok = pAdvertising->start();
    iotsaBLE_notifyAdvertisingStateChanged(ok);
    _noteAdvertisingStartResult(ok, 0);
    return;
  } else {
    duration -= extraDurationForConnectingMillis;
    IFBLEDEBUG IotsaSerial.printf("BLE resume advertising for %d ms\n", duration);
    bool ok = pAdvertising->start(duration);
    iotsaBLE_notifyAdvertisingStateChanged(ok);
    _noteAdvertisingStartResult(ok, (uint32_t)duration);
  }
}

void IotsaBLEServerMod::setup() {
  isEnabled = true;
  createServer();
  configLoad();
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
  return true;
}

bool IotsaBLEServerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonObject reqObj = request.as<JsonObject>();
  bool anyChanged = false;
  bool newEnabled = isEnabled;
  if (getFromRequest<int>(reqObj, "isEnabled", newEnabled) && newEnabled != isEnabled) {
    anyChanged = true;
    isEnabled = request["isEnabled"];
    iotsaConfig.requestReboot(4000);
  }
  if (getFromRequest<int>(reqObj, "adv_min", adv_min)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "adv_max", adv_max)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "tx_power_dbm", tx_power_dbm)) anyChanged = true;
  if (anyChanged) configSave();
  checkUnhandled(reqObj);
  return anyChanged;
}

void IotsaBLEServerMod::serverSetup() {
  api.setup("bleserver", true, true);
  name = "bleserver";
}

void IotsaBLEServerMod::lateSetupDone() {
  _startServer();
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
  if (NimBLEDevice::isInitialized()) {
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->stop();
    advertisingRetryAtMillis = 0; // about to explicitly restart below
    if (adv_min >= 0) pAdvertising->setMinInterval(adv_min);
    if (adv_max >= 0) pAdvertising->setMaxInterval(adv_max);
    _applyTxPower();
    bool ok = pAdvertising->start();
    _noteAdvertisingStartResult(ok, 0);
  }
}

void IotsaBLEServerMod::loop() {
  if (iotsaConfig.wantBleModeSwitchAtMillis > 0 && iotsaConfig.wantBleModeSwitchAtMillis < millis()) {
      IFBLEDEBUG IotsaSerial.println("BLE mode switch requested");
    //
    // Either setup() or saveConfig() or configuration mode change asked to change the BLE mode. Do so.
    //
    iotsaConfig.wantBleModeSwitchAtMillis = 0;
    _bleGotoMode();
  }
  if (advertisingRetryAtMillis != 0 && millis() >= advertisingRetryAtMillis) {
    advertisingRetryAtMillis = 0;
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising != nullptr) {
      IFBLEDEBUG IotsaSerial.println("BLE retry start advertising");
      uint32_t duration = advertisingRetryDuration;
      bool ok = (duration == 0) ? pAdvertising->start() : pAdvertising->start(duration);
      iotsaBLE_notifyAdvertisingStateChanged(ok);
      _noteAdvertisingStartResult(ok, duration);
    }
  }
}

void IotsaBleApiService::setup(const char* serviceUUID, IotsaBLEProvider *_apiProvider) {
  bool isAdvertising = IotsaBLEServerMod::pauseServer();
  IotsaBLEServerMod::createServer();
  next = IotsaBLEServerMod::s_services;
  IotsaBLEServerMod::s_services = this;
  apiProvider = _apiProvider;
  IFBLEDEBUG IotsaSerial.printf("IotsaBleApiService: create ble service %s to 0x%x\n", serviceUUID, (uint32_t)apiProvider);
  bleService = IotsaBLEServerMod::s_server->createService(serviceUUID);

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  // xxxjack wrong: if (isAdvertising) IotsaBLEServerMod::resumeServer();
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