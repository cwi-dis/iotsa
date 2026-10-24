#include "iotsa.h"
#include "iotsaBLEClient.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaConfigFile.h"

//
// IotsaBLEClientMod is intended to be used as a base class
// for other modules (which will then save configurations, etc, for the
// devices the module is interested in).
//

const int SCAN_START_RETRY_MS = 1000; // How long to wait before retrying start scan

bool IotsaBLEClientMod::coordinateWithServer = false;

void IotsaBLEClientMod::loadClientConfig() {
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  clientConfigLoad(cf);
}

void IotsaBLEClientMod::saveClientConfig() {
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  clientConfigSave(cf);
}

void IotsaBLEClientMod::clientConfigLoad(IotsaConfigFileLoad& cf) {
  cf.get("scan_interval", scan_interval, scan_interval);
  cf.get("scan_window", scan_window, scan_window);
  cf.get("scan_duration_discovery", scanDurationDiscoveryMillis, scanDurationDiscoveryMillis);
  cf.get("scan_cooldown_discovery", scanCooldownDiscoveryMillis, scanCooldownDiscoveryMillis);
  cf.get("connect_settle_time", connectSettleTimeMillis, connectSettleTimeMillis);
  cf.get("connect_timeout", connectTimeoutMillis, connectTimeoutMillis);
}

void IotsaBLEClientMod::clientConfigSave(IotsaConfigFileSave& cf) {
  cf.put("scan_interval", scan_interval);
  cf.put("scan_window", scan_window);
  cf.put("scan_duration_discovery", scanDurationDiscoveryMillis);
  cf.put("scan_cooldown_discovery", scanCooldownDiscoveryMillis);
  cf.put("connect_settle_time", connectSettleTimeMillis);
  cf.put("connect_timeout", connectTimeoutMillis);
}

void IotsaBLEClientMod::setup() {
  IFDEBUG IotsaSerial.println("BLEClientmod::setup()");
  loadClientConfig();
  iotsaBLE_ensureInitialized();
  setupScanner();
}

void IotsaBLEClientMod::setupScanner() {
  // The scanner is a singleton. We initialize it once.
  scanner = NimBLEDevice::getScan();
  scanner->setScanCallbacks(this, false);
  scanner->setActiveScan(true);
  scanner->setInterval(scan_interval);
  scanner->setWindow(scan_window);
  scanner = NULL;

}

void IotsaBLEClientMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("scan_interval", scan_interval);
  ctx.show("scan_window", scan_window);
  ctx.show("scan_duration_discovery", scanDurationDiscoveryMillis);
  ctx.show("scan_cooldown_discovery", scanCooldownDiscoveryMillis);
  ctx.show("connect_settle_time", connectSettleTimeMillis);
  ctx.show("connect_timeout", connectTimeoutMillis);
}

// Subclasses read more fields after calling this.
void IotsaBLEClientMod::putHandler(IotsaApiContext& ctx) {
  // How this device looks for the BLE devices it controls.
  IotsaContext::RightsScope rights(ctx);
  ctx.argsNeedRight("bleclient");
  bool changed = ctx.arg("scan_interval", scan_interval, IotsaApiContext::Saved);
  changed |= ctx.arg("scan_window", scan_window, IotsaApiContext::Saved);
  changed |= ctx.arg("scan_duration_discovery", scanDurationDiscoveryMillis, IotsaApiContext::Saved);
  changed |= ctx.arg("scan_cooldown_discovery", scanCooldownDiscoveryMillis, IotsaApiContext::Saved);
  changed |= ctx.arg("connect_settle_time", connectSettleTimeMillis, IotsaApiContext::Saved);
  changed |= ctx.arg("connect_timeout", connectTimeoutMillis, IotsaApiContext::Saved);
  // Our own file, not configSave() (see saveClientConfig()).
  if (changed) saveClientConfig();
  if (ctx.failed()) return;
  if (changed) setupScanner();
}

bool IotsaBLEClientMod::isScanning() {
  return scanner != nullptr && scanner->isScanning();
}

unsigned int IotsaBLEClientMod::maxConnectionKeepOpen() {
  if (!scanUpdate.armed()) return noScheduledScanKeepOpenCapMillis;
  return scanUpdate.remainingMillis();
}

bool IotsaBLEClientMod::mayOpenLink() {
  int open = 0;
  for (auto it: devices) {
    auto st = it.second->getLinkState();
    if (st == IotsaBLEClientDevice::LinkState::Connecting || st == IotsaBLEClientDevice::LinkState::Lingering) open++;
  }
  return open < maxOpenClientConnections;
}

bool IotsaBLEClientMod::needsDiscovery() {
  // We need active discovery if any known device has never been matched by
  // name yet (no address at all), or a known device just failed a connect
  // attempt and needs reconfirming. (IotsaBLEClientManagerMod adds "or
  // we're hunting for unknown devices" on top of this.)
  for (auto it: devices) {
    if (it.second->available() && !it.second->needsRescan) continue;
    if (scanOnlyForPendingWork && !it.second->hasPendingWork()) continue;
    return true;
  }
  return false;
}

void IotsaBLEClientMod::updateScanning() {
  if (isScanning()) {
    // Stop as soon as there's nothing left to look for -- no need to run out
    // a full scan duration once every reason to scan has been satisfied.
    if (!needsDiscovery()) stopScanning();
    return;
  }
  // A connect in progress (connections take priority over scanning) or
  // holdOffNewWork() (OTA) -- see IotsaBLERadioArbiter. Don't start a scan
  // now, but don't forget to look again either.
  if (!IotsaBLERadioArbiter::canBeginScan()) {
    scanUpdate.arm(SCAN_START_RETRY_MS);
    return;
  }
  if (!needsDiscovery()) return;
  IFDEBUG {
    IotsaSerial.print("BLE scan for: ");
    for (auto it: devices) {
      if (!it.second->available() || it.second->needsRescan) {
        IotsaSerial.printf("%s ", it.second->getName().c_str());
      }
    }
    const char *other = otherDiscoveryReason();
    if (other) IotsaSerial.printf("(%s)", other);
    IotsaSerial.println();
  }
  startScanning();
}

void IotsaBLEClientMod::startScanning() {
  if (isScanning()) {
    IotsaSerial.println("IotsaBLEClientMod.startScanning: already scanning...");
    return;
  }
  if (!IotsaBLERadioArbiter::tryBeginScan()) {
    // Lost the race against a connect that started since updateScanning()'s peek.
    scanUpdate.arm(SCAN_START_RETRY_MS);
    return;
  }
  IFDEBUG IotsaSerial.println("IotsaBLEClientMod: BLE scan start");
  if (coordinateWithServer) {
    IotsaBLERadioArbiter::pauseAdvertising(IotsaBLERadioArbiter::PAUSE_SCAN);
  }
  // Now start the scan
  uint32_t duration = scanDurationDiscoveryMillis;
  scanner = NimBLEDevice::getScan();
  scanningMod = this;
  scanStartedAtMillis = millis();
  bool startOk = scanner->start(duration);
  iotsaBLE_notifyScanningStateChanged(startOk && scanner->isScanning());
  if (!startOk) {
    scanner = nullptr;
    scanningMod = NULL;
    IotsaBLERadioArbiter::endActivity(IotsaBLERadioArbiter::ACTIVITY_SCAN);
    // Lift the pause again: stopScanning() won't run for a scan that never
    // started, so advertising would otherwise stay off until the next scan.
    if (coordinateWithServer) {
      IotsaBLERadioArbiter::resumeAdvertising(IotsaBLERadioArbiter::PAUSE_SCAN);
    }
    IFDEBUG IotsaSerial.println("NimBLEClient: cannot start scan, retry in 1s");
    scanUpdate.arm(SCAN_START_RETRY_MS);
    return;
  }
  scanningChanged();
}

void IotsaBLEClientMod::stopScanning() {
  if (scanner == nullptr) {
    IFDEBUG IotsaSerial.println("IotsaBLEClientMod.stopScanning: not scanning...");
  } else {
    IFDEBUG IotsaSerial.println("IotsaBLEClientMod.stopScanning: BLE scan stop");
    scanner->stop();
    scanStoppedAtMillis = millis();
    scanner = NULL;
    scanningMod = NULL;
    IotsaBLERadioArbiter::endActivity(IotsaBLERadioArbiter::ACTIVITY_SCAN);
    iotsaBLE_notifyScanningStateChanged(false);
    if (coordinateWithServer) {
      IotsaBLERadioArbiter::resumeAdvertising(IotsaBLERadioArbiter::PAUSE_SCAN);
    }
    scanningChanged();
  }
  // Next time through loop, check whether we should scan again.
  scanUpdate.arm(scanCooldownDiscoveryMillis);
}

bool IotsaBLEClientMod::canConnect() {
  // Cheap non-atomic peek, see IotsaBLERadioArbiter (cwi-dis/iotsa#263): no
  // scan or other connect in progress, settle time after the last scan
  // passed, no OTA hold-off, a slot left for the server role if it's active.
  return IotsaBLERadioArbiter::canBeginConnect(connectSettleTimeMillis);
}

void IotsaBLEClientMod::requestStopScanningForConnect() {
  // May be called from any task (e.g. BLEDimmer::connectionTask()). Do not
  // touch scanner/scanningMod here -- just flag it, loop() does the actual
  // stopScanning() call, same pattern as onScanEnd()/scanHasEnded below.
  scanStopRequested = true;
}

bool IotsaBLEClientMod::tryAcquireConnectSlot() {
  return IotsaBLERadioArbiter::tryBeginConnect(connectSettleTimeMillis);
}

void IotsaBLEClientMod::releaseConnectSlot() {
  IotsaBLERadioArbiter::endActivity(IotsaBLERadioArbiter::ACTIVITY_CONNECT);
}

void IotsaBLEClientMod::requestScanUpdate() {
  scanUpdate.arm(0);
}

IotsaBLEClientMod* IotsaBLEClientMod::scanningMod = NULL;

void IotsaBLEClientMod::onScanEnd(const NimBLEScanResults& scanResults, int reason) {
    // Called on the NimBLE host task, not the main loop() task. Do not touch
    // scanner/scanningMod or anything else non-trivial here -- just flag it
    // and let loop() (single-threaded) do the actual work.
    IFDEBUG IotsaSerial.printf("IotsaBLEClientMod: BLE scan complete, reason=%d\n", reason);
    scanHasEnded = true;
}

void IotsaBLEClientMod::lateSetup() {
  api.setup("bleclient", true, true, false);
  name = "bleclient";
}

void IotsaBLEClientMod::setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback) {
  knownDeviceCallback = _callback;
}

void IotsaBLEClientMod::loop() {
  // scanStopRequested and scanHasEnded may have been set by other tasks
  // (BLEDimmer::connectionTask() and the NimBLE host task, respectively).
  // Consume both here and perform at most one stopScanning() call -- this is
  // the only place scanner/scanningMod are ever written, so there is no
  // cross-task race on them.
  bool wantStop = scanStopRequested;
  scanStopRequested = false;
  if (scanHasEnded) {
    scanHasEnded = false;
    wantStop = true;
  } else if (scanner != nullptr && !scanner->isScanning()) {
    // Fallback in case onScanEnd() is ever missed.
    wantStop = true;
  }
  if (wantStop && scanner != nullptr) {
    stopScanning();
  }
  if (scanUpdate.fired()) {
    updateScanning();
  }
  // The one driver of every device's connection state machine
  // (cwi-dis/iotsa#263 step 2): nothing else advances it.
  bool someoneWaitsForLink = false;
  for (auto it: devices) {
    it.second->service();
    if (it.second->getLinkState() == IotsaBLEClientDevice::LinkState::WaitingForRadio) someoneWaitsForLink = true;
  }
  // A device is waiting and we're at maxOpenClientConnections: close a
  // lingering link early, the waiting device connects on a later pass.
  if (someoneWaitsForLink && !mayOpenLink()) {
    for (auto it: devices) {
      if (it.second->getLinkState() == IotsaBLEClientDevice::LinkState::Lingering && !it.second->hasPendingWork()) {
        IFDEBUG IotsaSerial.printf("IotsaBLEClientMod: closing lingering link to %s, another device needs the radio\n", it.second->getName().c_str());
        it.second->endLinger();
        break;
      }
    }
  }
}

void IotsaBLEClientMod::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
  // NimBLE host task: the device maps and the callbacks belong to the loop
  // task, so hand it a copy (cwi-dis/iotsa#236). If the queue is full the
  // advertisement is dropped; the device advertises again.
  NimBLEAdvertisedDevice copy(*advertisedDevice);
  IotsaApplication::postToLoop([this, copy]() { _handleAdvertisement(&copy); });
}

void IotsaBLEClientMod::_handleAdvertisement(const NimBLEAdvertisedDevice *advertisedDevice) {
#ifdef IOTSA_DEBUG_BLE_PRINT_ALL_CLIENTS
  IotsaSerial.printf("BLEClientMod::onResult(%s, RSSI: %d)\n", advertisedDevice->toString().c_str(), advertisedDevice->getRSSI());
#endif
  // Is this an advertisement for a device we know, either by name or by address?
  // (NimBLE-Arduino 2.1.0 stopped advertising the device name by default, so a
  // known device may well show up with no name at all -- the address match below
  // has to be reachable even then.)
  std::string deviceName = advertisedDevice->getName();
  // Some devices pad their advertised name with NUL bytes, which would then
  // never match a known device by name and show up as garbage in
  // /api/bleclient. Cut it at the first NUL (cwi-dis/iotsa#170).
  size_t nul = deviceName.find('\0');
  if (nul != std::string::npos) deviceName.erase(nul);
  if (deviceName != "") {
    auto it = devices.find(deviceName);
    if (it != devices.end()) {
      auto dev = it->second;
      if (dev == nullptr) {
        IotsaSerial.printf("BLEClientMod: device byName \"%s\" is NULL\n", deviceName.c_str());
        return;
      }
      bool changed = dev->receivedAdvertisement(*advertisedDevice);
      if (changed) {
        devicesByAddress[advertisedDevice->getAddress().toString()] = dev;
        IFDEBUG IotsaSerial.printf("BLEClientMod: advertisement update byname for %s\n", deviceName.c_str());
        onDeviceAddressChanged(dev);
        if (knownDeviceCallback) knownDeviceCallback(*advertisedDevice);
      }
      scanUpdate.arm(0); // We may have found what we were looking for
      return;
    }
  }
  std::string addr = advertisedDevice->getAddress().toString();
  auto it2 = devicesByAddress.find(addr);
  if (it2 != devicesByAddress.end()) {
    auto dev = it2->second;
    if (dev == nullptr) {
      IotsaSerial.printf("BLEClientMod: device byAddress \"%s\" is NULL\n", addr.c_str());
      return;
    }
    bool changed = dev->receivedAdvertisement(*advertisedDevice);
    if (changed) {
      devicesByAddress[advertisedDevice->getAddress().toString()] = dev;
      IFDEBUG IotsaSerial.printf("BLEClientMod: advertisement update byaddress for %s\n", addr.c_str());
      if (knownDeviceCallback) knownDeviceCallback(*advertisedDevice);
    }
    scanUpdate.arm(0); // We may have found what we were looking for
    return;
  }
  if (deviceName == "") return;
  onUnknownDeviceSeen(advertisedDevice, deviceName);
}

IotsaBLEClientDevice* IotsaBLEClientMod::addDevice(std::string id, IotsaBLEClientDevice* device) {
  scanUpdate.arm(0); // We probably want to scan for the new device
  auto it = devices.find(id);
  if (it == devices.end()) {
    if (device == nullptr) return nullptr;
    device->owner = this;
    devices[id] = device;
    return device;
  }
  return it->second;
}

IotsaBLEClientDevice* IotsaBLEClientMod::getDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    return NULL;
  }
  return it->second;
}

void IotsaBLEClientMod::noteKnownAddress(std::string id, std::string address) {
  if (address == "") return;
  IotsaBLEClientDevice *dev = getDevice(id);
  if (dev == NULL) return;
  dev->setKnownAddress(address);
  devicesByAddress[address] = dev;
  scanUpdate.arm(0); // We may be able to connect right away
}

void IotsaBLEClientMod::delDevice(std::string id) {
  scanUpdate.arm(0);  // We may be able to stop scanning
  auto it = devices.find(id);
  if (it != devices.end()) {
    // Also drop the devicesByAddress entry, if any -- otherwise it's left
    // dangling (onResult() dereferences it on a future matching
    // advertisement) once the device object itself goes away, whether via
    // its own destructor (DimmerBLEClient removes itself this way) or a
    // generic REST/web "remove" action (cwi-dis/iotsa#264). Found while
    // adding the latter -- previously the only caller removed an object it
    // was about to delete itself, so this was a real but neverbefore-
    // exercised-for-long risk.
    std::string address = it->second->getAddress();
    if (address != "") devicesByAddress.erase(address);
    devices.erase(it);
  }
}
#endif // IOTSA_WITH_BLE
