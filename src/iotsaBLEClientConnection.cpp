#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEClient.h"

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name, std::string _address)
: IotsaBLEDeviceInfo(_name, _address)
{
  connCallbacks.owner = this;
}

void IotsaBLEClientConnection::ConnCallbacks::onConnect(BLEClient* pClient) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onConnect\n", owner ? owner->getName().c_str() : "?");
}

void IotsaBLEClientConnection::ConnCallbacks::onDisconnect(BLEClient* pClient, int reason) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onDisconnect reason=%d (%s)\n",
    owner ? owner->getName().c_str() : "?", reason, NimBLEUtils::returnCodeToString(reason));
  if (owner) {
    // disconnectSettled is only ever set false by our own disconnect() call,
    // right before asking NimBLE to tear the link down. If it's still true
    // here, we never asked for this -- the connection went away on its own.
    if (owner->disconnectSettled) {
      owner->numConnectionFailed++;
    } else {
      owner->numConnectionClosedLocally++;
    }
    owner->disconnectSettled = true;
    owner->lastDisconnectReason = reason;
    owner->lastDisconnectAtMillis = millis();
  }
}

IotsaBLEClientConnection::~IotsaBLEClientConnection() {
  if (pClient) {
    BLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
}

bool IotsaBLEClientConnection::receivedAdvertisement(const BLEAdvertisedDevice& _device) {
  bool changed = IotsaBLEDeviceInfo::receivedAdvertisement(_device);
  // Seeing this device advertise at all reconfirms it's reachable, regardless
  // of whether its address happened to change.
  needsRescan = false;
  // disconnect() only touches pClient, not address/addressValid -- fine to
  // call after the base class has released addressMutex, and keeps
  // disconnect() (which talks to the BLE stack) from ever running while
  // addressMutex is held.
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
  if (xSemaphoreTake(addressMutex, addressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientConnection::connect: address mutex timeout, skipped");
    return false;
  }
  valid = addressValid;
  if (valid) {
    addr = address;
  }
  xSemaphoreGive(addressMutex);
  if (!valid) return false;
  numConnectCalls++;
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
    pClient->setClientCallbacks(&connCallbacks, false); // false: we own connCallbacks, don't delete it
  }
  if (pClient->isConnected()) {
    numConnectSkipped++;
    return true;
  }
  numConnectAttempts++;
  uint32_t t0 = millis();
  // A genuine new connect attempt starts here (the already-connected
  // fast-path above already returned) -- record it, and tally the outcome
  // below. Distinct from lastDisconnectReason, which only ever gets set on a
  // connection that *did* succeed and later went away.
  lastConnectAttemptAtMillis = t0;
  // Connections take priority over scanning: tell the owning mod a connect
  // attempt is in flight so updateScanning() holds off starting a new scan
  // until it's done (see IotsaBLEClientMod::noteConnectAttemptStarted()).
  if (owner) owner->noteConnectAttemptStarted();
  bool rv = pClient->connect(addr, false); // Keep previously learned services
  if (owner) owner->noteConnectAttemptEnded();
  uint32_t elapsedMs = millis() - t0;
  if (rv) {
    numConnectSucceeded++;
    needsRescan = false;
  } else {
    numConnectFailed++;
    IotsaSerial.printf("IotsaBLEClientConnection::connect(%s): failed after %ums, rc=%d (%s)\n",
      addr.toString().c_str(), elapsedMs, pClient->getLastError(), NimBLEUtils::returnCodeToString(pClient->getLastError()));
    // Don't clearDevice() here: a failed connect doesn't mean the address is
    // wrong (e.g. a lightSleep device just happened to be asleep mid-attempt)
    // -- just that we're not sure it's still reachable. needsRescan triggers
    // a rescan to reconfirm, without throwing away a known-good address.
    needsRescan = true;
    // Wake the scan scheduler: without this, nothing re-evaluates
    // needsDiscovery() until some unrelated event happens to touch
    // shouldUpdateScanAtMillis, so needsRescan could go unnoticed indefinitely.
    if (owner) owner->requestScanUpdate();
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

void IotsaBLEClientConnection::getHandler(JsonObject& reply) {
  IotsaBLEDeviceInfo::getHandler(reply);
  if (lastConnectAttemptAtMillis != 0) {
    reply["lastConnectAttemptMillisAgo"] = millis() - lastConnectAttemptAtMillis;
  }
  reply["numConnectCalls"] = numConnectCalls;
  reply["numConnectSkipped"] = numConnectSkipped;
  reply["numConnectAttempts"] = numConnectAttempts;
  reply["numConnectFailed"] = numConnectFailed;
  reply["numConnectSucceeded"] = numConnectSucceeded;
  reply["numConnectionOpen"] = isConnected() ? 1 : 0;
  reply["numConnectionFailed"] = numConnectionFailed;
  reply["numConnectionClosedLocally"] = numConnectionClosedLocally;
  if (lastDisconnectReason != -1) {
    reply["lastDisconnectReason"] = NimBLEUtils::returnCodeToString(lastDisconnectReason);
    reply["lastDisconnectMillisAgo"] = millis() - lastDisconnectAtMillis;
  }
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
