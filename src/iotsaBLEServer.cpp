#include "iotsa.h"
#include "iotsaBLEServer.h"
#include "iotsaConfigFile.h"
#ifdef IOTSA_WITH_BLE
#include <BLE2902.h>

#undef IOTSA_BLE_DEBUG
#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif

class IotsaBLEServerCallbacks : public BLEServerCallbacks {
	void onConnect(BLEServer* pServer) {
    IFBLEDEBUG IotsaSerial.printf("BLE connect conn_id=%d\n", pServer->getConnId());
    iotsaConfig.pauseSleep();
  }
	void onDisconnect(BLEServer* pServer) {
    IFBLEDEBUG IotsaSerial.printf("BLE Disconnect conn_id=%d\n", pServer->getConnId());
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
    IFBLEDEBUG IotsaSerial.printf("BLE char onRead 0x%x\n", (uint32_t)pCharacteristic);
    api->bleGetHandler(charUUID);
  }
	void onWrite(BLECharacteristic* pCharacteristic) {
    IFBLEDEBUG IotsaSerial.printf("BLE char onWrite\n");
    api->blePutHandler(charUUID);
  }
	void onNotify(BLECharacteristic* pCharacteristic) {
    IFBLEDEBUG IotsaSerial.printf("BLE char onNotify\n");
  }
	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
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
  }
  if (anyChanged) configSave();

  
  String message = "<html><head><title>BLE Server module</title></head><body><h1>BLE Server module</h1>";
  message += "<form method='get'>";
  message += "BLE Enabled: <input type='text' name='isEnabled' value='" + String((int)isEnabled) + "'><br>";
  message += "Advertising interval (min): <input type='text' name='adv_min' value='" + String(adv_min) + "'> (default: 32, unit: 0.625ms, range: 32..16384)<br>";
  message += "Advertising interval (max): <input type='text' name='adv_max' value='" + String(adv_max) + "'> (default: 64, unit: 0.625ms, range: 32..16384)<br>";
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
uint16_t IotsaBLEServerMod::adv_min = 0;
uint16_t IotsaBLEServerMod::adv_max = 0;

void IotsaBLEServerMod::createServer() {
  if (s_server) return;
  iotsaConfig.ensureConfigLoaded();
  IFBLEDEBUG IotsaSerial.print("BLE hostname: ");
  IFBLEDEBUG IotsaSerial.println(iotsaConfig.hostName.c_str());
  // We de-init bluetooth and release classic-mode memory, in the hope
  // this frees some of the memory the bluedroid stack uses.
  BLEDevice::deinit(false);
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  BLEDevice::init(iotsaConfig.hostName.c_str());
  s_server = BLEDevice::createServer();
  s_server->setCallbacks(new IotsaBLEServerCallbacks());
}

void IotsaBLEServerMod::startServer() {
  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
  // And start services
  for (IotsaBleApiService *sp = s_services; sp; sp=sp->next) {
    sp->bleService->start();
  }
}

bool IotsaBLEServerMod::pauseServer() {
  if (s_server) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();
    return true;
  }
  return false;
}

void IotsaBLEServerMod::resumeServer() {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
}

void IotsaBLEServerMod::setup() {
  isEnabled = true;
  createServer();
  configLoad();
  if (!isEnabled) {
    BLEDevice::deinit(false);
    esp_bt_mem_release(ESP_BT_MODE_BTDM);
    return;
  }

  IFBLEDEBUG IotsaSerial.println("BLE server start advertising and services");
  startServer();
}

#ifdef IOTSA_WITH_API
bool IotsaBLEServerMod::getHandler(const char *path, JsonObject& reply) {
  reply["isEnabled"] = isEnabled;
  reply["adv_min"] = adv_min;
  reply["adv_max"] = adv_max;
  return true;
}

bool IotsaBLEServerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (request.containsKey("isEnabled") && request["isEnabled"] != isEnabled) {
    isEnabled = request["isEnabled"];
    iotsaConfig.requestReboot(4000);
  }
  adv_min = request["adv_min"]|adv_min;
  adv_max = request["adv_max"]|adv_max;
  configSave();
  return true;
}
#endif // IOTSA_WITH_API

void IotsaBLEServerMod::serverSetup() {
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
  if (adv_min) pAdvertising->setMinInterval(adv_min);
  cf.get("adv_max", adv_max, adv_max);
  if (adv_max) pAdvertising->setMaxInterval(adv_max);
}

void IotsaBLEServerMod::configSave() {
  IotsaConfigFileSave cf("/config/bleserver.cfg");
  cf.put("isEnabled", isEnabled);
  cf.put("adv_min", adv_min);
  cf.put("adv_max", adv_max);
  if (BLEDevice::getInitialized()) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();
    if (adv_min) pAdvertising->setMinInterval(adv_min);
    if (adv_max) pAdvertising->setMaxInterval(adv_max);
    pAdvertising->start();
  }
}

void IotsaBLEServerMod::loop() {
  static int firstLoop = 0;
  if (firstLoop == 0) {
    firstLoop = 1;
    IFDEBUG IotsaSerial.println("BLE server loop");
  }
}

void IotsaBleApiService::setup(const char* serviceUUID, IotsaBLEApiProvider *_apiProvider) {
  IotsaBLEServerMod::createServer();
  next = IotsaBLEServerMod::s_services;
  IotsaBLEServerMod::s_services = this;
  apiProvider = _apiProvider;
  IFBLEDEBUG IotsaSerial.printf("create ble service %s to 0x%x\n", serviceUUID, (uint32_t)apiProvider);
  bleService = IotsaBLEServerMod::s_server->createService(serviceUUID);

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
}

void IotsaBleApiService::addCharacteristic(UUIDstring charUUID, int mask, BLEDescriptor *d1, BLEDescriptor *d2, BLEDescriptor *d3) {
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
  if (d1) newChar->addDescriptor(d1);
  if (d2) newChar->addDescriptor(d2);
  if (d3) newChar->addDescriptor(d3);

  characteristicUUIDs[nCharacteristic-1] = charUUID;
  bleCharacteristics[nCharacteristic-1] = newChar;
}

void IotsaBleApiService::set(UUIDstring charUUID, const uint8_t *data, size_t size) {
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      bleCharacteristics[i]->setValue((uint8_t *)data, size);
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


void IotsaBleApiService::getAsBuffer(UUIDstring charUUID, uint8_t **datap, size_t *sizep) {
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      auto value = bleCharacteristics[i]->getValue();
      if (datap) *datap = (uint8_t *)value.c_str();
      if (sizep) *sizep = value.size();
      return;
    }
  }
  IotsaSerial.println("get: unknown characteristic");
}

int IotsaBleApiService::getAsInt(UUIDstring charUUID) {
  size_t size;
  uint8_t *ptr;
  int val = 0;
  int shift = 0;
  getAsBuffer(charUUID, &ptr, &size);
  while (size--) {
    val = val | (*ptr++ << shift);
    shift += 8;
  }
  return val;
}

std::string IotsaBleApiService::getAsString(UUIDstring charUUID) {
  size_t size;
  uint8_t *ptr;
  getAsBuffer(charUUID, &ptr, &size);
  return std::string((const char *)ptr, size);
}


#endif // IOTSA_WITH_BLE