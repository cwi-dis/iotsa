#include "iotsaBLEClientDevice.h"

#ifdef IOTSA_WITH_BLE
#include "iotsaBLEClient.h"

// A failed connect only means the bleAddress is genuinely worth reconfirming via a
// rescan if we haven't actually seen this device's own advertisement in a while --
// if it's still advertising regularly (this recently), it's reachable at the
// discovery level and another scan won't help whatever's actually failing the GAP
// connect procedure itself. Generous enough to span several duty-cycle wake windows
// of a light-sleep device (see lissabon/CLAUDE.md's 1500ms sleep/300ms wake) without
// forcing an extra scan on top of the normal periodic one. Confirmed live,
// 2026-09-26: three lissabonController dimmers failing every connect attempt but
// still advertising fine were triggering a rescan roughly every 10s regardless.
static const uint32_t RESCAN_STALENESS_MS = 30000;

IotsaBLEClientDevice::IotsaBLEClientDevice(const std::string& _name, std::string _bleAddress, IotsaBLEClientMod* _owner)
: IotsaBLEDeviceInfo(_name, _bleAddress),
  owner(_owner)
{
  connCallbacks.owner = this;
}

void IotsaBLEClientDevice::ConnCallbacks::onConnect(NimBLEClient* pClient) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientDevice(%s): onConnect\n", owner ? owner->getName().c_str() : "?");
  // Host task: only hand the outcome to service(). (Also fires for the
  // blocking connect() path, where linkState isn't Connecting -- ignored.)
  if (owner && owner->linkState == LinkState::Connecting) owner->asyncConnectResult = 1;
}

void IotsaBLEClientDevice::ConnCallbacks::onConnectFail(NimBLEClient* pClient, int reason) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientDevice(%s): onConnectFail reason=%d (%s)\n",
    owner ? owner->getName().c_str() : "?", reason, NimBLEUtils::returnCodeToString(reason));
  if (owner && owner->linkState == LinkState::Connecting) {
    owner->asyncConnectFailReason = reason;
    owner->asyncConnectResult = -1;
  }
}

void IotsaBLEClientDevice::ConnCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
  IFDEBUG IotsaSerial.printf("IotsaBLEClientDevice(%s): onDisconnect reason=%d (%s)\n",
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
    // Link established, then lost before NimBLE reported it fully connected.
    if (owner->linkState == LinkState::Connecting && owner->asyncConnectResult == 0) {
      owner->asyncConnectFailReason = reason;
      owner->asyncConnectResult = -1;
    }
  }
}

IotsaBLEClientDevice::~IotsaBLEClientDevice() {
  release();
}

void IotsaBLEClientDevice::release() {
  // Gives the NimBLEClient slot back to the shared pool (NimBLEDevice's own
  // fixed-size m_pClients array, sized by NIMBLE_MAX_CONNECTIONS) instead of
  // holding it for the lifetime of this object. Safe to call while still
  // connected/disconnecting -- NimBLEDevice::deleteClient() defers the actual
  // delete until any in-flight disconnect completes. Callers that idle out a
  // connection (see BLEDimmer's disconnectAtMillis handling) should call this
  // instead of disconnect(), so a device with more IotsaBLEClientDevices
  // than the platform has slots for can still round-robin through all of them
  // (cwi-dis/iotsa#106-era gap, found live on lissabonController, 2026-09-25).
  if (pClient) {
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
}

bool IotsaBLEClientDevice::receivedAdvertisement(const NimBLEAdvertisedDevice& _device) {
  bool changed = IotsaBLEDeviceInfo::receivedAdvertisement(_device);
  // Seeing this device advertise at all reconfirms it's reachable, regardless
  // of whether its bleAddress happened to change.
  needsRescan = false;
  // disconnect() only touches pClient, not bleAddress/bleAddressValid -- fine to
  // call after the base class has released bleAddressMutex, and keeps
  // disconnect() (which talks to the BLE stack) from ever running while
  // bleAddressMutex is held.
  if (changed) disconnect();
  return changed;
}

void IotsaBLEClientDevice::clearDevice() {
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientDevice::clearDevice: bleAddress mutex timeout, skipped");
  } else {
    bleAddressValid = false;
    xSemaphoreGive(bleAddressMutex);
  }
  disconnect();
}

bool IotsaBLEClientDevice::available() {
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientDevice::available: bleAddress mutex timeout");
    return false;
  }
  bool rv = bleAddressValid;
  xSemaphoreGive(bleAddressMutex);
  return rv;
}

bool IotsaBLEClientDevice::canConnect() {
  if (owner) owner->requestStopScanningForConnect();
  return owner ? owner->canConnect() : true;
}

bool IotsaBLEClientDevice::connect() {
  // Snapshot bleAddress (and bleAddressType) under the lock, then release it
  // before doing anything BLE-related -- pClient->connect() below can block
  // for up to the owning mod's connectTimeoutMillis and must never run
  // while bleAddressMutex is held.
  bool valid = false;
  NimBLEAddress addr("", 0);
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) {
    IotsaSerial.println("IotsaBLEClientDevice::connect: bleAddress mutex timeout, skipped");
    return false;
  }
  valid = bleAddressValid;
  if (valid) {
    addr = bleAddress;
  }
  xSemaphoreGive(bleAddressMutex);
  if (!valid) return false;
  numConnectCalls++;
  if (pClient == nullptr) {
    pClient = NimBLEDevice::createClient(addr);
    if (pClient == nullptr) {
      // NimBLEDevice::createClient() returns nullptr once NIMBLE_MAX_CONNECTIONS
      // client slots are all in use -- expected, transient contention when
      // more IotsaBLEClientDevices exist than the platform has slots for
      // (each one now releases its slot on idle disconnect, see release()),
      // not a bug. Fail this attempt gracefully instead of dereferencing a
      // null pClient below (was a StoreProhibited crash, live on lissabonController
      // with 5 dimmers and NIMBLE_MAX_CONNECTIONS=3, 2026-09-25).
      numConnectAttempts++;
      numConnectFailed++;
      IotsaSerial.println("IotsaBLEClientDevice::connect: no free BLE client slot, will retry");
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
    pClient->setConnectionParams(LINK_INTERVAL_MIN, LINK_INTERVAL_MAX, 0, LINK_SUPERVISION_TIMEOUT);
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
    IotsaSerial.printf("IotsaBLEClientDevice::connect(%s): failed after %ums, rc=%d (%s)\n",
      addr.toString().c_str(), elapsedMs, pClient->getLastError(), NimBLEUtils::returnCodeToString(pClient->getLastError()));
    // Don't clearDevice() here: a failed connect doesn't mean the bleAddress is
    // wrong (e.g. a lightSleep device just happened to be asleep mid-attempt)
    // -- just that we're not sure it's still reachable. needsRescan triggers
    // a rescan to reconfirm, without throwing away a known-good bleAddress. But
    // only bother if we haven't actually seen it advertise recently -- if we
    // have, the failure is at the GAP-connect step itself, not discovery, and
    // another scan won't fix that (see RESCAN_STALENESS_MS above).
    // Re-enabled (2026-09-26, cwi-dis/iotsa#263): was disabled to test whether
    // a scan starting shortly after a failed connect was itself disturbing the
    // radio for the *next* attempt, beyond the already-handled immediate
    // scan-vs-connect exclusion (connectSettleTimeMillis, only 100ms). The
    // slot-leak bug that confounded that original test (see release() below)
    // is fixed now, and the real culprit turned out to be the rc=2 TOCTOU
    // race (fixed by tryAcquireConnectSlot()), not rescan-after-failure --
    // safe to turn back on.
    if (millis() - getLastSeenAtMillis() > RESCAN_STALENESS_MS) {
      needsRescan = true;
      // Wake the scan scheduler: without this, nothing re-evaluates
      // needsDiscovery() until some unrelated event happens to touch
      // shouldUpdateScanAtMillis, so needsRescan could go unnoticed indefinitely.
      if (owner) owner->requestScanUpdate();
    }
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

void IotsaBLEClientDevice::disconnect() {
  if (pClient && pClient->isConnected()) {
    disconnectSettled = false;
    pClient->disconnect();
  }
}

bool IotsaBLEClientDevice::isConnected() {
  return pClient && pClient->isConnected();
}

bool IotsaBLEClientDevice::isDisconnecting() {
  return !disconnectSettled;
}

// How long past the connect timeout service() waits for NimBLE to report the
// outcome of an asynchronous connect before giving up on it itself. NimBLE
// applies the timeout and reports it through onConnectFail(); this is only a
// backstop against a callback that never comes.
static const uint32_t ASYNC_CONNECT_BACKSTOP_MS = 2000;

void IotsaBLEClientDevice::requestWork(uint32_t deadlineMs) {
  workDeadlineAtMillis = millis() + deadlineMs;
  workPending = true;
  // The address may need finding first.
  if (owner) owner->requestScanUpdate();
}

void IotsaBLEClientDevice::service() {
  uint32_t now = millis();
  if (linkState == LinkState::Connecting) {
    int8_t result = asyncConnectResult;
    if (result == 0) {
      // Work deadline passed mid-connect: cancel, onConnectFail() follows
      // and the give-up below happens on a later pass.
      if (workPending && (int32_t)(now - workDeadlineAtMillis) >= 0 && pClient && !cancelRequested) {
        cancelRequested = true;
        pClient->cancelConnect();
      }
      uint32_t timeout = (owner ? owner->getConnectTimeoutMillis() : 6000) + ASYNC_CONNECT_BACKSTOP_MS;
      if (now - connectStartedAtMillis < timeout) return;
      IotsaSerial.printf("IotsaBLEClientDevice(%s): no connect outcome after %ums, giving up\n", bleName.c_str(), (unsigned)(now - connectStartedAtMillis));
      if (owner) owner->releaseConnectSlot();
      _connectFailed(BLE_HS_ETIMEOUT);
      return;
    }
    if (owner) owner->releaseConnectSlot();
    if (result < 0) {
      _connectFailed(asyncConnectFailReason);
      return; // work stays pending: retried until its deadline
    }
    numConnectSucceeded++;
    needsRescan = false;
    linkState = LinkState::Lingering; // connected; the work runs below
    lingerUntilMillis = now;
  }
  if (linkState == LinkState::Lingering && !isConnected()) {
    // The peer (or the link) went away on its own.
    release();
    linkState = LinkState::Idle;
  }
  if (!workPending) {
    if (linkState == LinkState::Lingering) {
      if ((int32_t)(now - lingerUntilMillis) >= 0) _closeLink();
    } else {
      linkState = LinkState::Idle;
    }
    return;
  }
  if ((int32_t)(now - workDeadlineAtMillis) >= 0) {
    workPending = false;
    lastWorkStatus = "gave up: device not reachable";
    IotsaSerial.printf("IotsaBLEClientDevice(%s): %s\n", bleName.c_str(), lastWorkStatus);
    workAbandoned();
    if (linkState != LinkState::Lingering) linkState = LinkState::Idle;
    // A scan may have been running only for this work: let it stop now.
    if (owner) owner->requestScanUpdate();
    return;
  }
  if (linkState == LinkState::Lingering) {
    _runWork();
    return;
  }
  if (!available() || needsRescan) {
    // No address, or a failed connect and we haven't seen it advertise
    // lately: wait for a scan to (re)confirm it rather than retrying the
    // connect blindly -- a retry would also cut that scan short.
    linkState = LinkState::WaitingForAddress;
    return;
  }
  if (isConnected()) {
    // Someone (the blocking connect() path) left a link open: just use it.
    numConnectCalls++;
    numConnectSkipped++;
    linkState = LinkState::Lingering;
    _runWork();
    return;
  }
  linkState = LinkState::WaitingForRadio;
  if (isDisconnecting()) return;  // NimBLE rejects a connect while the previous disconnect settles
  if (owner && !owner->mayOpenLink()) return;  // the driver will cut a lingering link short for us
  if (!canConnect()) return;  // radio busy (also asks a running scan to stop)
  if (_startAsyncConnect()) linkState = LinkState::Connecting;
}

void IotsaBLEClientDevice::_runWork() {
  // Clear first: a request arriving while doWork() runs is new work.
  workPending = false;
  uint32_t t0 = millis();
  bool ok = doWork();
  lastWorkMillis = millis() - t0;
  if (lastWorkMillis > maxWorkMillis) maxWorkMillis = lastWorkMillis;
  lastWorkStatus = ok ? "done" : "failed";
  uint32_t keepOpen = keepOpenMillis;
  if (owner && keepOpen > owner->maxConnectionKeepOpen()) keepOpen = owner->maxConnectionKeepOpen();
  lingerUntilMillis = millis() + keepOpen;
  if (keepOpen) iotsaController.postponeSleep(keepOpen + 1000);
}

bool IotsaBLEClientDevice::_startAsyncConnect() {
  // Same bookkeeping as the blocking connect() below, minus the wait.
  NimBLEAddress addr("", 0);
  if (xSemaphoreTake(bleAddressMutex, bleAddressMutexTimeout) != pdTRUE) return false;
  bool valid = bleAddressValid;
  if (valid) addr = bleAddress;
  xSemaphoreGive(bleAddressMutex);
  if (!valid) return false;
  if (pClient == nullptr) {
    pClient = NimBLEDevice::createClient(addr);
    if (pClient == nullptr) {
      // All NIMBLE_MAX_CONNECTIONS client slots in use: transient, retry later.
      return false;
    }
    pClient->setConnectTimeout(owner ? owner->getConnectTimeoutMillis() : 6000);
    pClient->setConnectionParams(LINK_INTERVAL_MIN, LINK_INTERVAL_MAX, 0, LINK_SUPERVISION_TIMEOUT);
    pClient->setClientCallbacks(&connCallbacks, false);
  }
  if (owner && !owner->tryAcquireConnectSlot()) return false;
  numConnectCalls++;
  numConnectAttempts++;
  connectStartedAtMillis = lastConnectAttemptAtMillis = millis();
  asyncConnectResult = 0;
  cancelRequested = false;
  // linkState must be Connecting before the callbacks can fire.
  linkState = LinkState::Connecting;
  if (!pClient->connect(addr, false, true)) { // keep learned services, asynchronous
    if (owner) owner->releaseConnectSlot();
    _connectFailed(pClient->getLastError());
    return false;
  }
  return true;
}

void IotsaBLEClientDevice::_connectFailed(int rc) {
  numConnectFailed++;
  IotsaSerial.printf("IotsaBLEClientDevice(%s): connect failed after %ums, rc=%d (%s)\n",
    bleName.c_str(), (unsigned)(millis() - connectStartedAtMillis), rc, NimBLEUtils::returnCodeToString(rc));
  // Same reasoning as in connect(): only rescan if we haven't seen the device
  // advertise lately, and give the client slot back.
  if (millis() - getLastSeenAtMillis() > RESCAN_STALENESS_MS) {
    needsRescan = true;
    if (owner) owner->requestScanUpdate();
  }
  release();
  linkState = LinkState::Idle;
}

void IotsaBLEClientDevice::_closeLink() {
  disconnect();  // marks it as a local close, for the connection statistics
  release();
  linkState = LinkState::Idle;
}

void IotsaBLEClientDevice::endLinger() {
  if (linkState == LinkState::Lingering && !workPending) _closeLink();
}

void IotsaBLEClientDevice::getHandler(JsonObject& reply) {
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
  reply["found"] = available();
  reply["connected"] = isConnected();
  static const char *linkStateNames[] = {"idle", "waitingForAddress", "waitingForRadio", "connecting", "lingering"};
  reply["linkState"] = linkStateNames[(int)linkState];
  reply["workPending"] = (bool)workPending;
  if (lastWorkStatus) reply["lastWorkStatus"] = lastWorkStatus;
  reply["lastWorkMillis"] = lastWorkMillis;
  reply["maxWorkMillis"] = maxWorkMillis;
}

bool IotsaBLEClientDevice::retarget(const std::string& newName) {
  if (newName == bleName) return false;
  if (owner && bleName != "") owner->delDevice(bleName);
  setKnownName(newName);
  clearDevice(); // old address/connection state doesn't apply to the new target
  if (owner && bleName != "") owner->addDevice(bleName, this);
  return true;
}

bool IotsaBLEClientDevice::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  std::string cfgName;
  cf.get(f_name + ".name", cfgName, "");
  if (cfgName != "") retarget(cfgName);
  std::string cfgAddress;
  cf.get(f_name + ".address", cfgAddress, "");
  if (cfgAddress != "") {
    // Through owner (if we have one) rather than a direct setKnownAddress(),
    // so IotsaBLEClientMod's devicesByAddress index stays correct too --
    // see noteKnownAddress().
    if (owner) owner->noteKnownAddress(bleName, cfgAddress);
    else setKnownAddress(cfgAddress);
  }
  return bleName != "";
}

void IotsaBLEClientDevice::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  cf.put(f_name + ".name", bleName);
  std::string addr = getAddress();
  if (addr != "") cf.put(f_name + ".address", addr);
}

bool IotsaBLEClientDevice::putHandler(const JsonVariant& request) {
  if (!request.is<JsonObject>()) return false;
  const JsonObject& reqObj = request.as<JsonObject>();
  String newName;
  if (getFromRequest<String>(reqObj, "name", newName)) {
    return retarget(std::string(newName.c_str()));
  }
  return false;
}

#ifdef IOTSA_WITH_WEB
void IotsaBLEClientDevice::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  message += text;
  if (includeConfig) {
    message += "BLE device name: <input name='" + f_name + ".name' value='" + String(bleName.c_str()) + "'><br>";
  }
  if (available()) {
    message += "BLE address " + String(getAddress().c_str());
    message += isConnected() ? " (connected)" : " (found, not connected)";
  } else {
    message += "<em>not found</em>";
  }
  message += "<br>";
}

void IotsaBLEClientDevice::formHandler_TD(String& message, bool includeConfig) {
  message += "<td>" + String(bleName.c_str()) + "</td><td>";
  if (available()) {
    message += String(getAddress().c_str());
    message += isConnected() ? " (connected)" : "";
  } else {
    message += "<em>not found</em>";
  }
  message += "</td>";
}

bool IotsaBLEClientDevice::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  if (!includeConfig) return false;
  String n_name = f_name + ".name";
  if (server->hasArg(n_name)) {
    return retarget(std::string(server->arg(n_name).c_str()));
  }
  return false;
}
#endif // IOTSA_WITH_WEB

NimBLERemoteCharacteristic *IotsaBLEClientDevice::_getCharacteristic(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID) {
  NimBLERemoteService *service = pClient->getService(serviceUUID);
  if (service == NULL) return NULL;
  NimBLERemoteCharacteristic *characteristic = service->getCharacteristic(charUUID);
  return characteristic;
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const uint8_t *data, size_t size) {
  NimBLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canWrite()) return false;
  characteristic->writeValue(data, size);
  return true;
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 1);
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 2);
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t value) {
  return set(serviceUUID, charUUID, (const uint8_t *)&value, 4);
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const std::string& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientDevice::set(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, const String& value) {
  return set(serviceUUID, charUUID, (const uint8_t *)value.c_str(), value.length());
}

bool IotsaBLEClientDevice::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, std::string& value) {
  NimBLERemoteCharacteristic *characteristic = _getCharacteristic(serviceUUID, charUUID);
  if (characteristic == NULL) return false;
  if (!characteristic->canRead()) return false;
  // Copy into the caller's string: readValue() returns a temporary, so a
  // pointer into it (what the old getAsBuffer() handed out) dangles as soon
  // as this function returns.
  value = characteristic->readValue();
  return true;
}

// Typed reads: the characteristic value must be exactly sizeof(T) bytes.
// memcpy() rather than a pointer cast, the string's buffer need not be
// aligned for T.
template<typename T> static bool _getTyped(IotsaBLEClientDevice *dev, NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, T& value) {
  std::string buf;
  if (!dev->get(serviceUUID, charUUID, buf)) return false;
  if (buf.size() != sizeof(T)) return false;
  memcpy(&value, buf.data(), sizeof(T));
  return true;
}

bool IotsaBLEClientDevice::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint8_t& value) {
  return _getTyped(this, serviceUUID, charUUID, value);
}

bool IotsaBLEClientDevice::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint16_t& value) {
  return _getTyped(this, serviceUUID, charUUID, value);
}

bool IotsaBLEClientDevice::get(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, uint32_t& value) {
  return _getTyped(this, serviceUUID, charUUID, value);
}

static BleNotificationCallback _staticCallback;

static void _staticCallbackCaller(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (_staticCallback) _staticCallback(pData, length);
}

bool IotsaBLEClientDevice::getAsNotification(NimBLEUUID& serviceUUID, NimBLEUUID& charUUID, BleNotificationCallback callback) {
  if (_staticCallback != NULL) {
    IotsaSerial.println("IotsaBLEClientDevice: only a single notification supported");
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
