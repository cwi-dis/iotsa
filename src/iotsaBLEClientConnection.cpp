#include "iotsaBLEClientConnection.h"

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEClient.h"

// A failed connect only means the address is genuinely worth reconfirming via a
// rescan if we haven't actually seen this device's own advertisement in a while --
// if it's still advertising regularly (this recently), it's reachable at the
// discovery level and another scan won't help whatever's actually failing the GAP
// connect procedure itself. Generous enough to span several duty-cycle wake windows
// of a light-sleep device (see lissabon/CLAUDE.md's 1500ms sleep/300ms wake) without
// forcing an extra scan on top of the normal periodic one. Confirmed live,
// 2026-09-26: three lissabonController dimmers failing every connect attempt but
// still advertising fine were triggering a rescan roughly every 10s regardless.
static const uint32_t RESCAN_STALENESS_MS = 30000;

IotsaBLEClientConnection::IotsaBLEClientConnection(std::string& _name, std::string _address)
: IotsaBLEDeviceInfo(_name, _address)
{
  connCallbacks.owner = this;
}

void IotsaBLEClientConnection::ConnCallbacks::onConnect(NimBLEClient* pClient) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientConnection(%s): onConnect\n", owner ? owner->getName().c_str() : "?");
}

void IotsaBLEClientConnection::ConnCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
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
  release();
}

void IotsaBLEClientConnection::release() {
  // Gives the NimBLEClient slot back to the shared pool (NimBLEDevice's own
  // fixed-size m_pClients array, sized by NIMBLE_MAX_CONNECTIONS) instead of
  // holding it for the lifetime of this object. Safe to call while still
  // connected/disconnecting -- NimBLEDevice::deleteClient() defers the actual
  // delete until any in-flight disconnect completes. Callers that idle out a
  // connection (see BLEDimmer's disconnectAtMillis handling) should call this
  // instead of disconnect(), so a device with more IotsaBLEClientConnections
  // than the platform has slots for can still round-robin through all of them
  // (cwi-dis/iotsa#106-era gap, found live on lissabonController, 2026-09-25).
  if (pClient) {
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
}

bool IotsaBLEClientConnection::receivedAdvertisement(const NimBLEAdvertisedDevice& _device) {
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
  NimBLEAddress addr("", 0);
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
    pClient = NimBLEDevice::createClient(addr);
    if (pClient == nullptr) {
      // NimBLEDevice::createClient() returns nullptr once NIMBLE_MAX_CONNECTIONS
      // client slots are all in use -- expected, transient contention when
      // more IotsaBLEClientConnections exist than the platform has slots for
      // (each one now releases its slot on idle disconnect, see release()),
      // not a bug. Fail this attempt gracefully instead of dereferencing a
      // null pClient below (was a StoreProhibited crash, live on lissabonController
      // with 5 dimmers and NIMBLE_MAX_CONNECTIONS=3, 2026-09-25).
      numConnectAttempts++;
      numConnectFailed++;
      IotsaSerial.println("IotsaBLEClientConnection::connect: no free BLE client slot, will retry");
      needsRescan = true;
      if (owner) owner->requestScanUpdate();
      return false;
    }
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
  // Acquire the single device-wide connect slot atomically right before the
  // actual attempt -- this, not canConnect()'s earlier (non-atomic) peek, is
  // what makes concurrent callers race-free (cwi-dis/iotsa#263). A caller
  // that loses the race is told "not right now," same as a canConnect()==false
  // outcome, and doesn't get counted as a real failed attempt below.
  if (owner && !owner->tryAcquireConnectSlot()) {
    return false;
  }
  numConnectAttempts++;
  uint32_t t0 = millis();
  // A genuine new connect attempt starts here (the already-connected
  // fast-path above already returned) -- record it, and tally the outcome
  // below. Distinct from lastDisconnectReason, which only ever gets set on a
  // connection that *did* succeed and later went away.
  lastConnectAttemptAtMillis = t0;
  bool rv = pClient->connect(addr, false); // Keep previously learned services
  if (owner) owner->releaseConnectSlot();
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
    // a rescan to reconfirm, without throwing away a known-good address. But
    // only bother if we haven't actually seen it advertise recently -- if we
    // have, the failure is at the GAP-connect step itself, not discovery, and
    // another scan won't fix that (see RESCAN_STALENESS_MS above).
    // EXPERIMENTAL (2026-09-26): disabled entirely to test whether the scan
    // itself is what's actually hurting connect success -- a scan starting
    // shortly after a failed connect could be disturbing the radio for the
    // *next* attempt too, beyond just the already-handled immediate
    // scan-vs-connect exclusion (connectSettleTimeMillis, only 100ms). If
    // this measurably improves things, work out a real fix (e.g. a longer
    // settle time) instead of leaving rescan off for good.
#if 0
    if (millis() - getLastSeenAtMillis() > RESCAN_STALENESS_MS) {
      needsRescan = true;
      // Wake the scan scheduler: without this, nothing re-evaluates
      // needsDiscovery() until some unrelated event happens to touch
      // shouldUpdateScanAtMillis, so needsRescan could go unnoticed indefinitely.
      if (owner) owner->requestScanUpdate();
    }
#endif
    // Give the slot back on a failed attempt too, not just after a successful
    // connect's idle-disconnect (see release()) -- otherwise the first N
    // dimmers to ever attempt a connect (whether that attempt succeeds or
    // not) permanently claim all NIMBLE_MAX_CONNECTIONS slots between them,
    // since a failed connect left pClient non-null and every retry just
    // reused the same client. Any dimmer that hadn't gotten its first
    // attempt in yet before that happened could then never get one, ever
    // (confirmed live on lissabonController, 2026-09-26: stripdeur/stripbank
    // hit "no free BLE client slot" on literally every subsequent attempt,
    // for the rest of that boot, while three other dimmers -- including ones
    // that never actually succeeded -- silently held the only 3 slots).
    release();
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

NimBLERemoteCharacteristic *IotsaBLEClientConnection::_getCharacteristic(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID) {
  NimBLERemoteService *service = pClient->getService(serviceUUID);
  if (service == NULL) return NULL;
  NimBLERemoteCharacteristic *characteristic = service->getCharacteristic(charUUID);
  return characteristic;
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const uint8_t *data, size_t size) {
  NimBLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canWrite()) return false;
  characteristic->writeValue(data, size);
  return true;
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 1);
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 2);
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 4);
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const std::string& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const String& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientConnection::getAsBuffer(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t **datap, size_t *sizep) {
  NimBLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canRead()) return false;
  std::string value = characteristic->readValue();
  *datap = (uint8_t *)value.c_str();
  *sizep = value.length();
  return true;
}
bool IotsaBLEClientConnection::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint8_t)) return false;
  value = *(uint8_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint16_t)) return false;
  value = *(uint16_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  if (size != sizeof(uint32_t)) return false;
  value = *(uint32_t *)ptr;
  return true;
}

bool IotsaBLEClientConnection::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, std::string& value) {
  size_t size;
  uint8_t *ptr;
  if (!getAsBuffer(serviceUUID, charUUID, &ptr, &size)) return false;
  value = std::string((const char *)ptr, size);
  return true;
}

static BleNotificationCallback _staticCallback;

static void _staticCallbackCaller(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (_staticCallback) _staticCallback(pData, length);
}

bool IotsaBLEClientConnection::getAsNotification(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, BleNotificationCallback callback) {
  if (_staticCallback != NULL) {
    IotsaSerial.println("IotsaBLEClientConnection: only a single notification supported");
    return false;
  }
  NimBLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canNotify()) return false;
  _staticCallback = callback;
  characteristic->subscribe(true, _staticCallbackCaller);
  return false;
}
#endif // IOTSA_WITH_BLE
