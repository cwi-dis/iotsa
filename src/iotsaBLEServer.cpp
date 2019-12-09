#include "iotsa.h"
#include "iotsaBLEServer.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_BLE

#undef IOTSA_BLE_DEBUG
#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif

#ifdef IOTSA_BLE_DEBUG
class IotsaBLEServerCallbacks : public BLEServerCallbacks {
	void onConnect(BLEServer* pServer) {
    IotsaSerial.println("BLE connect\n");
  }
	void onDisconnect(BLEServer* pServer) {
    IotsaSerial.println("BLE Disconnect\n");

  }
};
#endif // IOTSA_BLE_DEBUG

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
  message += "Advertising interval (min): <input type='text' name='adv_min' value='" + String(adv_min) + "'> (default: 32, unit: 0.625ms, range: 32..16384)<br>";
  message += "Advertising interval (max): <input type='text' name='adv_max' value='" + String(adv_max) + "'> (default: 32, unit: 0.625ms, range: 32..16384)<br>";
  message += "<input type='submit'></form></body></html>";
  server->send(200, "text/html", message);
}

String IotsaBLEServerMod::info() {
  String message = "<p>Built with BLE server module. See <a href=\"/bleserver\">/bleserver</a> to change settings.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

BLEServer *IotsaBLEServerMod::s_server = 0;

void IotsaBLEServerMod::createServer() {
  if (s_server) return;
  BLEDevice::init(iotsaConfig.hostName.c_str());
  s_server = BLEDevice::createServer();
#ifdef IOTSA_BLE_DEBUG
  s_server->setCallbacks(new IotsaBLEServerCallbacks());
#endif
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
}

void IotsaBLEServerMod::setup() {
  createServer();
  configLoad();
}

#ifdef IOTSA_WITH_API
bool IotsaBLEServerMod::getHandler(const char *path, JsonObject& reply) {
  reply["adv_min"] = adv_min;
  reply["adv_max"] = adv_max;
  return true;
}

bool IotsaBLEServerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  adv_min = request["adv_min"]|0;
  adv_max = request["adv_max"]|0;
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
  pAdvertising->stop();
  IotsaConfigFileLoad cf("/config/bleserver.cfg");
  int value;
  cf.get("adv_min", value, 0);
  adv_min = value;
  if (adv_min) pAdvertising->setMinInterval(adv_min);
  cf.get("adv_max", value, 0);
  adv_max = value;
  if (adv_max) pAdvertising->setMaxInterval(adv_max);
  pAdvertising->start();
}

void IotsaBLEServerMod::configSave() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->stop();
  IotsaConfigFileSave cf("/config/bleserver.cfg");
  cf.put("adv_min", adv_min);
  cf.put("adv_max", adv_max);
  if (adv_min) pAdvertising->setMinInterval(adv_min);
  if (adv_max) pAdvertising->setMaxInterval(adv_max);
  pAdvertising->start();
}

void IotsaBLEServerMod::loop() {
}

void IotsaBleApiService::setup(const char* serviceUUID, IotsaBLEApiProvider *_apiProvider) {
  IotsaBLEServerMod::createServer();
  apiProvider = _apiProvider;
  IFBLEDEBUG IotsaSerial.printf("ble service %s to 0x%x\n", serviceUUID, (uint32_t)apiProvider);
  bleService = IotsaBLEServerMod::s_server->createService(serviceUUID);
  bleService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->stop();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->start();
}

void IotsaBleApiService::addCharacteristic(UUIDstring charUUID, int mask) {
  IFBLEDEBUG IotsaSerial.printf("ble characteristic %s mask %d\n", charUUID, mask);
  nCharacteristic++;
  characteristicUUIDs = (UUIDstring *)realloc((void *)characteristicUUIDs, nCharacteristic*sizeof(UUIDstring));
  bleCharacteristics = (BLECharacteristic **)realloc((void *)bleCharacteristics, nCharacteristic*sizeof(BLECharacteristic *));
  if (characteristicUUIDs == NULL || bleCharacteristics == NULL) {
    IotsaSerial.println("addCharacteristic out of memory");
    return;
  }
  bleService->stop();
  BLECharacteristic *newChar = bleService->createCharacteristic(charUUID, mask);
  newChar->setCallbacks(new IotsaBLECharacteristicCallbacks(charUUID, apiProvider));

  characteristicUUIDs[nCharacteristic-1] = charUUID;
  bleCharacteristics[nCharacteristic-1] = newChar;
  bleService->start();
}

void IotsaBleApiService::set(UUIDstring charUUID, const uint8_t *data, size_t size) {
  for(int i=0; i<nCharacteristic; i++) {
    if (characteristicUUIDs[i] == charUUID) {
      bleCharacteristics[i]->setValue((uint8_t *)data, size);
      return;
    }
    IotsaSerial.println("set: unknown characteristic");
  }
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
    IotsaSerial.println("set: unknown characteristic");
  }
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