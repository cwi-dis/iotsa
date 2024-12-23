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

class IotsaBLEServerCallbacks : public BLEServerCallbacks {
	void onConnect(BLEServer* pServer) {
    IFBLEDEBUG IotsaSerial.printf("BLE connect\n");
    iotsaConfig.pauseSleep();
  }
	void onDisconnect(BLEServer* pServer) {
    IFBLEDEBUG IotsaSerial.printf("BLE Disconnect\n");
    iotsaConfig.resumeSleep();
    pServer->startAdvertising();

  }
};

class IotsaBLECharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  IotsaBLECharacteristicCallbacks(UUIDstring _charUUID, IotsaBLEApiProvider *_api)
  : charUUID(_charUUID),
    api(_api)
  {}

	void onRead(BLECharacteristic* pCharacteristic) {
    IFBLEDEBUG IotsaSerial.printf("BLE char onRead %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaConfig.postponeSleep(0);
    api->bleGetHandler(charUUID);
  }
	void onWrite(BLECharacteristic* pCharacteristic) {
    IFBLEDEBUG IotsaSerial.printf("BLE char onWrite %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaConfig.postponeSleep(0);
    api->blePutHandler(charUUID);
  }
	void onNotify(BLECharacteristic* pCharacteristic) {
    IFBLEDEBUG IotsaSerial.printf("BLE char onNotify %s\n", pCharacteristic->getUUID().toString().c_str());
    iotsaConfig.postponeSleep(0);
  }
	void onStatus(BLECharacteristic* pCharacteristic, uint32_t code) {
    iotsaConfig.postponeSleep(0);
    IFBLEDEBUG IotsaSerial.printf("BLE char onStatus\n");
  }
private:
  UUIDstring charUUID;
  IotsaBLEApiProvider *api;
};

#ifdef IOTSA_WITH_WEB
void
IotsaBLEServerMod::handler() {
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
  }if( server->hasArg("tx_power")) {
    tx_power = strtol(server->arg("tx_power").c_str(), 0, 10);
    anyChanged = true;
  }
  if (anyChanged) configSave();

  
  String message = "<html><head><title>BLE Server module</title></head><body><h1>BLE Server module</h1>";
  message += "<form method='get'>";
  message += "BLE Enabled: <input type='text' name='isEnabled' value='" + String((int)isEnabled) + "'><br>";
  message += "Advertising interval (min): <input type='text' name='adv_min' value='" + String(adv_min) + "'> (default: -1, unit: 0.625ms, range: 32..16384)<br>";
  message += "Advertising interval (max): <input type='text' name='adv_max' value='" + String(adv_max) + "'> (default: -1, unit: 0.625ms, range: 32..16384)<br>";
  message += "Transmit power level: <input type='text' name='tx_power' value='" + String(tx_power) + "'> (default: -1, unit: 3dbm, range: 0..7 for -12dbm to 9dbm)<br>";
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

BLEServer *IotsaBLEServerMod::s_server = 0;
IotsaBleApiService *IotsaBLEServerMod::s_services = NULL;
int IotsaBLEServerMod::adv_min = -1;
int IotsaBLEServerMod::adv_max = -1;
int IotsaBLEServerMod::tx_power = -1;

void IotsaBLEServerMod::createServer() {
  if (s_server) return;
  iotsaConfig.ensureConfigLoaded();
  IFBLEDEBUG IotsaSerial.print("BLE hostname: ");
  IFBLEDEBUG IotsaSerial.println(iotsaConfig.hostName.c_str());
  #ifndef IOTSA_WITH_NIMBLE
  // We de-init bluetooth and release classic-mode memory, in the hope
  // this frees some of the memory the bluedroid stack uses.
  BLEDevice::deinit(false);
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  #endif

  BLEDevice::init(iotsaConfig.hostName.c_str());
  if (tx_power >= 0) {
    BLEDevice::setPower((esp_power_level_t)tx_power);
  }
s_server = BLEDevice::createServer();
  s_server->setCallbacks(new IotsaBLEServerCallbacks());
}

void IotsaBLEServerMod::_startServer() {
  // Start services
  IFBLEDEBUG IotsaSerial.println("BLE start services");
  for (IotsaBleApiService *sp = s_services; sp; sp=sp->next) {
    sp->bleService->start();
  }
  if (iotsaConfig.bleDisabledOnBoot) {
    iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_DISABLED;
  } else {
    iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_ENABLED;
  }
  _bleGotoMode();
}

void IotsaBLEServerMod::_bleGotoMode() {
  // if (s_server == nullptr) return;
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  if (pAdvertising == nullptr) return;
  bool wasActive = pAdvertising->isAdvertising();
  bool isActive = iotsaConfig.bleMode == iotsa_ble_mode::IOTSA_BLE_ENABLED;
  if (wasActive == isActive) {
    IFBLEDEBUG IotsaSerial.printf("BLE advertising is already %d\n", int(wasActive));
    //return;
  }
  if (isActive) {
    IFBLEDEBUG IotsaSerial.println("BLE start advertising");
    // causes crash: esp_bt_controller_enable(esp_bt_mode_t::ESP_BT_MODE_BLE);
    pAdvertising->start();
  } else {
    IFBLEDEBUG IotsaSerial.println("BLE stop advertising");
    pAdvertising->stop();
    // re-enabling causes crash: esp_bt_controller_disable();
  }
}

bool IotsaBLEServerMod::pauseServer() {
  // For now we keep pauseServer() and resumeServer(), because the use case is for light sleep.
  if (s_server) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising == nullptr || !pAdvertising->isAdvertising()) return false;
    IFBLEDEBUG IotsaSerial.println("BLE pause advertising");
    pAdvertising->stop();
    return true;
  }
  return false;
}

void IotsaBLEServerMod::resumeServer() {
  IFBLEDEBUG IotsaSerial.println("BLE resume advertising");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
}

void IotsaBLEServerMod::setup() {
  isEnabled = true;
  createServer();
  configLoad();
  if (!isEnabled) {
    IFBLEDEBUG IotsaSerial.println("BLE deinit, not isEnabled");
    BLEDevice::deinit(false);
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    return;
  }
}

#ifdef IOTSA_WITH_API
bool IotsaBLEServerMod::getHandler(const char *path, JsonObject& reply) {
  reply["isEnabled"] = isEnabled;
  reply["adv_min"] = adv_min;
  reply["adv_max"] = adv_max;
  reply["tx_power"] = tx_power;
  return true;
}

bool IotsaBLEServerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonObject reqObj = request.as<JsonObject>();
  bool newEnabled = isEnabled;
  if (getFromRequest<bool>(reqObj, "isEnabled", newEnabled) && newEnabled != isEnabled) {
    isEnabled = request["isEnabled"];
    iotsaConfig.requestReboot(4000);
  }
  adv_min = request["adv_min"]|adv_min;
  adv_max = request["adv_max"]|adv_max;
  tx_power = request["tx_power"]|tx_power;
  configSave();
  return true;
}
#endif // IOTSA_WITH_API

void IotsaBLEServerMod::serverSetup() {
  _startServer();
#ifdef IOTSA_WITH_WEB
  server->on("/bleserver", std::bind(&IotsaBLEServerMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/bleserver", true, true);
  name = "bleserver";
#endif
}

void IotsaBLEServerMod::configLoad() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  IotsaConfigFileLoad cf("/config/bleserver.cfg");
  cf.get("isEnabled", isEnabled, true);
  cf.get("adv_min", adv_min, adv_min);
  if (adv_min >= 0) pAdvertising->setMinInterval(adv_min);
  cf.get("adv_max", adv_max, adv_max);
  if (adv_max >= 0) pAdvertising->setMaxInterval(adv_max);
  cf.get("tx_power", tx_power, tx_power);
  if (tx_power >= 0) {
    BLEDevice::setPower((esp_power_level_t)tx_power);
  }
}

void IotsaBLEServerMod::configSave() {
  IotsaConfigFileSave cf("/config/bleserver.cfg");
  cf.put("isEnabled", isEnabled);
  cf.put("adv_min", adv_min);
  cf.put("adv_max", adv_max);
  cf.put("tx_power", tx_power);
  if (BLEDevice::isInitialized()) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();
    if (adv_min >= 0) pAdvertising->setMinInterval(adv_min);
    if (adv_max >= 0) pAdvertising->setMaxInterval(adv_max);
    if (tx_power >= 0) {
      BLEDevice::setPower((esp_power_level_t)tx_power);
    }
    pAdvertising->start();
  }
}

void IotsaBLEServerMod::loop() {
    if (iotsaConfig.wantBleModeSwitchAtMillis > 0 && iotsaConfig.wantBleModeSwitchAtMillis < millis()) {
      IFBLEDEBUG IotsaSerial.println("BLE mode switch requested");
    //
    // Either setup() or saveConfig() or configuration mode change asked to change the WiFi mode. Do so.
    //
    iotsaConfig.wantBleModeSwitchAtMillis = 0;
    _bleGotoMode();
  }
}

void IotsaBleApiService::setup(const char* serviceUUID, IotsaBLEApiProvider *_apiProvider) {
  bool isAdvertising = IotsaBLEServerMod::pauseServer();
  IotsaBLEServerMod::createServer();
  next = IotsaBLEServerMod::s_services;
  IotsaBLEServerMod::s_services = this;
  apiProvider = _apiProvider;
  IFBLEDEBUG IotsaSerial.printf("create ble service %s to 0x%x\n", serviceUUID, (uint32_t)apiProvider);
  bleService = IotsaBLEServerMod::s_server->createService(serviceUUID);

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  if (isAdvertising) IotsaBLEServerMod::resumeServer();
}

void IotsaBleApiService::addCharacteristic(UUIDstring charUUID, int mask, uint8_t d2904format, uint16_t d2904unit, const char *d2901descr) {
  IFBLEDEBUG IotsaSerial.printf("add ble characteristic %s mask %d\n", charUUID, mask);
  nCharacteristic++;
  characteristicUUIDs = (UUIDstring *)realloc((void *)characteristicUUIDs, nCharacteristic*sizeof(UUIDstring));
  bleCharacteristics = (BLECharacteristic **)realloc((void *)bleCharacteristics, nCharacteristic*sizeof(BLECharacteristic *));
  if (characteristicUUIDs == NULL || bleCharacteristics == NULL) {
    IotsaSerial.println("addCharacteristic out of memory");
    return;
  }
  BLECharacteristic *newChar = bleService->createCharacteristic(charUUID, mask);
  newChar->setCallbacks(new IotsaBLECharacteristicCallbacks(charUUID, apiProvider));
#ifdef IOTSA_WITH_NIMBLE
  BLEDescriptor *d2901 = newChar->createDescriptor("2901");
  BLE2904 *d2904 = (BLE2904 *)newChar->createDescriptor("2904");
#else
  BLEDescriptor *d2901 = new BLEDescriptor("2901");
  BLE2904 *d2904 = new BLE2904();
#endif
  d2901->setValue(std::string(d2901descr));
  d2904->setFormat(d2904format);
  d2904->setUnit(d2904unit);
#ifndef IOTSA_WITH_NIMBLE
  newChar->addDescriptor(d2901);
  newChar->addDescriptor(d2904);
#endif

  characteristicUUIDs[nCharacteristic-1] = charUUID;
  bleCharacteristics[nCharacteristic-1] = newChar;
}

void IotsaBleApiService::set(UUIDstring charUUID, const uint8_t *data, size_t size) {
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      BLECharacteristic* ch = bleCharacteristics[i];
      ch->setValue((uint8_t *)data, size);
      bool want_notify = ch->getProperties() & NIMBLE_PROPERTY::NOTIFY;
      bool want_indicate = ch->getProperties() & NIMBLE_PROPERTY::INDICATE;
      if(want_notify || want_indicate) ch->notify((uint8_t *)data, size, want_notify);
      return;
    }
  }
  IotsaSerial.println("set: unknown characteristic");
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
  IotsaSerial.println("get: unknown characteristic");
  return "";
}


#endif // IOTSA_WITH_BLE