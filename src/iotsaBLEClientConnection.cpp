#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEClient.h"

// Never portMAX_DELAY: bounds how long any caller (including loop()) can
// possibly wait on addressMutex, so a stuck holder degrades to a skipped
// update, not a wedged task. The lock is only ever held for a plain field
// copy, so contention this long should never actually happen.
static const TickType_t addressMutexTimeout = pdMS_TO_TICKS(20);

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name, std::string _address)
: name(_name),
  addressMutex(xSemaphoreCreateMutex()),
  address(_address, 0), // Public is default for address type for nimble
#ifdef IOTSA_WITHOUT_NIMBLE
  addressType(BLE_ADDR_TYPE_PUBLIC),
#endif
  addressValid(false),
  pClient(nullptr)
{
  connCallbacks.owner = this;
  if (_address != "") {
    // address and addressType have already been set. Not yet shared with
    // any other task, so no need to take addressMutex here.
    addressValid = true;
  }
}

void IotsaBLEClientConnection::ConnCallbacks::onConnect(BLEClient* pClient) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onConnect\n", owner ? owner->getName().c_str() : "?");
}

#ifdef IOTSA_WITHOUT_NIMBLE
void IotsaBLEClientConnection::ConnCallbacks::onDisconnect(BLEClient* pClient) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onDisconnect\n", owner ? owner->getName().c_str() : "?");
  if (owner) owner->disconnectSettled = true;
}
#else
void IotsaBLEClientConnection::ConnCallbacks::onDisconnect(BLEClient* pClient, int reason) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onDisconnect reason=%d (%s)\n",
    owner ? owner->getName().c_str() : "?", reason, NimBLEUtils::returnCodeToString(reason));
  if (owner) owner->disconnectSettled = true;
}
#endif

IotsaBLEClientConnection::~IotsaBLEClientConnection() {
  if (pClient) {
    BLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
  vSemaphoreDelete(addressMutex);
}

void IotsaBLEClientConnection::setKnownAddress(const std::string& _address) {
  if (_address == "") return;
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::setKnownAddress: address mutex timeout, skipped");
    return;
  }
  if (!(addressValid && address.toString() == _address)) {
    address = BLEAddress(_address, 0); // Public is default for address type for nimble
    addressValid = true;
  }
  xSemaphoreGive(addressMutex);
}

std::string IotsaBLEClientConnection::getAddress() {
  std::string rv = "";
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::getAddress: address mutex timeout");
    return rv;
  }
  bool valid = addressValid;
#ifdef IOTSA_WITHOUT_NIMBLE
  valid = valid && addressType == BLE_ADDR_TYPE_PUBLIC;
#endif
  if (valid) rv = address.toString();
  xSemaphoreGive(addressMutex);
  return rv;
}

bool IotsaBLEClientConnection::receivedAdvertisement(const BLEAdvertisedDevice& _device) {
  lastSeenAtMillis = millis();
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::receivedAdvertisement: address mutex timeout, skipped");
    return false;
  }
  // Check whether the address is the same, then we don't have to add anything.
  bool sameAddress = addressValid
#ifdef IOTSA_WITHOUT_NIMBLE
    && _device.getAddressType() == addressType
#endif
    && _device.getAddress().equals(address);
  bool changed = false;
  if (!sameAddress) {
    address = _device.getAddress();
#ifdef IOTSA_WITHOUT_NIMBLE
    addressType = _device.getAddressType();
#endif
    addressValid = true;
    changed = true;
  }
  xSemaphoreGive(addressMutex);
  // disconnect() only touches pClient, not address/addressValid -- fine to
  // call after releasing the lock, and keeps disconnect() (which talks to
  // the BLE stack) from ever running while addressMutex is held.
  if (changed) disconnect();
  return changed;
}

void IotsaBLEClientConnection::clearDevice() {
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::clearDevice: address mutex timeout, skipped");
  } else {
    addressValid = false;
    xSemaphoreGive(addressMutex);
  }
  disconnect();
}

bool IotsaBLEClientConnection::available() {
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::available: address mutex timeout");
    return false;
  }
  bool rv = addressValid;
  xSemaphoreGive(addressMutex);
  return rv;
}

bool IotsaBLEClientConnection::connect() {
  // Snapshot address (and addressType) under the lock, then release it
  // before doing anything BLE-related -- pClient->connect() below can block
  // for up to the owning mod's connectTimeoutMillis and must never run
  // while addressMutex is held.
  bool valid = false;
  BLEAddress addr("", 0);
#ifdef IOTSA_WITHOUT_NIMBLE
  esp_ble_addr_type_t addrType = BLE_ADDR_TYPE_PUBLIC;
#endif
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::connect: address mutex timeout, skipped");
    return false;
  }
  valid = addressValid;
  if (valid) {
    addr = address;
#ifdef IOTSA_WITHOUT_NIMBLE
    addrType = addressType;
#endif
  }
  xSemaphoreGive(addressMutex);
  if (!valid) return false;
  if (pClient == nullptr) {
    pClient = BLEDevice::createClient(addr);
    // setConnectTimeout() takes milliseconds -- confirmed 2026-07-19 by
    // reading NimBLEClient.cpp's own doc comment ("The number of
    // milliseconds before timeout, default is 30 seconds", default
    // m_connectTimeout=30000). A previous version of this code passed a
    // value intended as seconds straight through with no conversion,
    // configuring a 6ms timeout instead of 6s -- every connect attempt
    // failed with BLE_HS_ETIMEOUT after ~10ms regardless of any
    // scanning/mutex issue. Now sourced (in ms) from the owning mod's
    // configurable connectTimeoutMillis instead of a hardcoded constant;
    // 6000 is only a fallback for the (should-never-happen) case of a
    // connection created without going through addDevice().
    pClient->setConnectTimeout(owner ? owner->getConnectTimeoutMillis() : 6000);
#ifdef IOTSA_WITHOUT_NIMBLE
    pClient->setClientCallbacks(&connCallbacks);
#else
    pClient->setClientCallbacks(&connCallbacks, false); // false: we own connCallbacks, don't delete it
#endif
  }
  if (pClient->isConnected()) return true;
  uint32_t t0 = millis();
  // Connections take priority over scanning: tell the owning mod a connect
  // attempt is in flight so updateScanning() holds off starting a new scan
  // until it's done (see IotsaBLEClientMod::noteConnectAttemptStarted()).
  if (owner) owner->noteConnectAttemptStarted();
#ifdef IOTSA_WITHOUT_NIMBLE
  bool rv = pClient->connect(addr, addrType);
#else
  bool rv = pClient->connect(addr, false); // Keep previously learned services
#endif
  if (owner) owner->noteConnectAttemptEnded();
  uint32_t elapsedMs = millis() - t0;
  if (!rv) {
#ifdef IOTSA_WITHOUT_NIMBLE
    IotsaSerial.printf("IotsaBLEClientConnection::connect(%s): failed after %ums\n", addr.toString().c_str(), elapsedMs);
#else
    IotsaSerial.printf("IotsaBLEClientConnection::connect(%s): failed after %ums, rc=%d (%s)\n",
      addr.toString().c_str(), elapsedMs, pClient->getLastError(), NimBLEUtils::returnCodeToString(pClient->getLastError()));
#endif
    clearDevice();
  }
  return rv;
}

void IotsaBLEClientConnection::disconnect() {
  if (pClient && pClient->isConnected()) {
    disconnectSettled = false;
    pClient->disconnect();
  }
}

bool IotsaBLEClientConnection::isConnected() {
  return pClient && pClient->isConnected();
}

bool IotsaBLEClientConnection::isDisconnecting() {
  return !disconnectSettled;
}

BLERemoteCharacteristic *IotsaBLEClientConnection::_getCharacteristic(BLEUUID& serviceUUID, BLEUUID& charUUID) {
  BLERemoteService *service = pClient->getService(serviceUUID);
  if (service == NULL) return NULL;
  BLERemoteCharacteristic *characteristic = service->getCharacteristic(charUUID);
  return characteristic;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const uint8_t *data, size_t size) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canWrite()) return false;
  characteristic->writeValue(data, size);
  return true;
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 1);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 2);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 4);
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const std::string& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::set(BLEUUID& serviceUUID, BLEUUID& charUUID, const String& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::getAsBuffer(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t **datap, size_t *sizep) {
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canRead()) return false;
  std::string value = characteristic->readValue();
  *datap = (uint8_t *)value.c_str();
  *sizep = value.length();
  return true;
}
bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint8_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint8_t)) return false;
  value = *(uint8_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint16_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint16_t)) return false;
  value = *(uint16_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, uint32_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint32_t)) return false;
  value = *(uint32_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(BLEUUID& serviceUUID, BLEUUID& charUUID, std::string& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  value = std::string((const char *)ptr, size);
  return true;
}

static BleNotificationCallback _staticCallback;

static void _staticCallbackCaller(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (_staticCallback) _staticCallback(pData, length);
}

bool IotsaBLEClientConnection::getAsNotification(BLEUUID& serviceUUID, BLEUUID& charUUID, BleNotificationCallback callback) {
  if (_staticCallback != NULL) {
    IotsaSerial.println("IotsaBLEClientConnection: only a single notification supported");
    return false;
  }
  BLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canNotify()) return false;
  _staticCallback = callback;
  characteristic->subscribe(true, _staticCallbackCaller);
  return false;
}
#endif // IOTSA_WITH_BLE
