#include "iotsa.h"
#include "iotsaBLEClient.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"
#include "iotsaRunmodeBLEClient.h"

//
// IotsaBLEClientMod is intended to be used as a base class
// for other modules (which will then save configurations, etc, for the
// devices the module is interested in).
//

const int SCAN_START_RETRY_MS = 1000; // How long to wait before retrying start scan

bool IotsaBLEClientMod::coordinateWithServer = false;

void IotsaBLEClientMod::loadScanConfig() {
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  cf.get("scan_interval", scan_interval, scan_interval);
  cf.get("scan_window", scan_window, scan_window);
  cf.get("scan_duration_discovery", scanDurationDiscoveryMillis, scanDurationDiscoveryMillis);
  cf.get("scan_cooldown_discovery", scanCooldownDiscoveryMillis, scanCooldownDiscoveryMillis);
  cf.get("connect_settle_time", connectSettleTimeMillis, connectSettleTimeMillis);
  cf.get("connect_timeout", connectTimeoutMillis, connectTimeoutMillis);
}

void IotsaBLEClientMod::saveScanConfig() {
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  cf.put("scan_interval", scan_interval);
  cf.put("scan_window", scan_window);
  cf.put("scan_duration_discovery", scanDurationDiscoveryMillis);
  cf.put("scan_cooldown_discovery", scanCooldownDiscoveryMillis);
  cf.put("connect_settle_time", connectSettleTimeMillis);
  cf.put("connect_timeout", connectTimeoutMillis);
}

void IotsaBLEClientMod::setup() {
  IFDEBUG IotsaSerial.println("BLEClientmod::setup()");
  loadScanConfig();
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

bool IotsaBLEClientMod::getHandler(const char *path, JsonObject& reply) {
  reply["scan_interval"] = scan_interval;
  reply["scan_window"] = scan_window;
  reply["scan_duration_discovery"] = scanDurationDiscoveryMillis;
  reply["scan_cooldown_discovery"] = scanCooldownDiscoveryMillis;
  reply["connect_settle_time"] = connectSettleTimeMillis;
  reply["connect_timeout"] = connectTimeoutMillis;
  return true;
}

bool IotsaBLEClientMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<int>(reqObj, "scan_interval", scan_interval)) {
    scan_interval = reqObj["scan_interval"];
    anyChanged = true;
  }
  if (getFromRequest<int>(reqObj, "scan_window", scan_window)) {
    scan_window = reqObj["scan_window"];
    anyChanged = true;
  }
  if (getFromRequest<int>(reqObj, "scan_duration_discovery", scanDurationDiscoveryMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "scan_cooldown_discovery", scanCooldownDiscoveryMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "connect_settle_time", connectSettleTimeMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "connect_timeout", connectTimeoutMillis)) anyChanged = true;
  if (anyChanged) {
    saveScanConfig();
    setupScanner();
  }
  return anyChanged;
}

bool IotsaBLEClientMod::isScanning() {
  return scanner != nullptr && scanner->isScanning();
}

unsigned int IotsaBLEClientMod::maxConnectionKeepOpen() {
  if (shouldUpdateScanAtMillis == 0) return noScheduledScanKeepOpenCapMillis;
  int32_t millisUntilScanDeadline = (int32_t)(shouldUpdateScanAtMillis - millis());
  if (millisUntilScanDeadline < 0) millisUntilScanDeadline = 0;
  return (unsigned int)millisUntilScanDeadline;
}

bool IotsaBLEClientMod::needsDiscovery() {
  // We need active discovery if any known device has never been matched by
  // name yet (no address at all), or a known device just failed a connect
  // attempt and needs reconfirming. (IotsaBLEClientCollectionMod adds "or
  // we're hunting for unknown devices" on top of this.)
  for (auto it: devices) {
    if (!it.second->available()) return true;
    if (it.second->needsRescan) return true;
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
  // Connections take priority over scanning: never start a new scan while
  // any connect attempt is in progress (starting one has been observed to
  // disrupt the in-flight connection at the link layer, even when NimBLE's
  // own scan-vs-connect exclusion correctly rejects the scan-start call).
  // Retry once the connect is done.
  if (connectingCount > 0) {
    shouldUpdateScanAtMillis = millis() + SCAN_START_RETRY_MS;
    return;
  }
  // WiFi-heavy work (OTA especially) asked us to hold off starting anything
  // new -- see IotsaBLERadioArbiter::holdOffNewWork(), cwi-dis/iotsa#263. Same retry
  // pattern as the connectingCount check above: don't start a scan now, but
  // don't forget to look again either.
  if (IotsaBLERadioArbiter::newWorkHeldOff()) {
    shouldUpdateScanAtMillis = millis() + SCAN_START_RETRY_MS;
    return;
  }
  if (!needsDiscovery()) return;
  IFDEBUG {
    IotsaSerial.print("BLE scan for: ");
    for (auto it: devices) {
      if (!it.second->available()) {
        IotsaSerial.printf("%s ", it.second->getName().c_str());
      }
    }
    IotsaSerial.println();
  }
  startScanning();
}

void IotsaBLEClientMod::startScanning() {
  if (isScanning()) {
    IotsaSerial.println("IotsaBLEClientMod.startScanning: already scanning...");
    return;
  }
  IFDEBUG IotsaSerial.println("IotsaBLEClientMod: BLE scan start");
  if (coordinateWithServer) {
    advertisingWasPausedByScan = IotsaBLEServerMod::pauseServer();
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
    IFDEBUG IotsaSerial.println("NimBLEClient: cannot start scan, retry in 1s");
    shouldUpdateScanAtMillis = millis() + SCAN_START_RETRY_MS;
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
    scanner = NULL;
    scanningMod = NULL;
    scanStoppedAtMillis = millis();
    iotsaBLE_notifyScanningStateChanged(false);
    if (coordinateWithServer && advertisingWasPausedByScan) {
      IotsaBLEServerMod::resumeServer();
      advertisingWasPausedByScan = false;
    }
    scanningChanged();
  }
  // Next time through loop, check whether we should scan again.
  shouldUpdateScanAtMillis = millis() + scanCooldownDiscoveryMillis;
}

bool IotsaBLEClientMod::canConnect() {
  // Connecting to a device while we are scanning has proved to result in issues
  // (confirmed live 2026-07-18: NimBLE's ble_gap_connect() outright rejects a
  // connection attempt while a scan is active). Also require a short settle
  // time after scanning stops -- an immediate connect right after stopScanning()
  // has also been observed to fail.
  if (scanner != NULL) return false;
  if (millis() - scanStoppedAtMillis < connectSettleTimeMillis) return false;
  // WiFi-heavy work (OTA especially) asked us to hold off starting anything
  // new -- see IotsaBLERadioArbiter::holdOffNewWork(), cwi-dis/iotsa#263.
  if (IotsaBLERadioArbiter::newWorkHeldOff()) return false;
  // EXPERIMENTAL (2026-09-25, cwi-dis/lissabon#30 follow-up): cap outgoing
  // connect attempts to one at a time, device-wide. Hypothesis: two
  // concurrent NimBLEClient::connect() calls contend for the same physical
  // radio at the link layer, corrupting/missing each other's packets, the
  // same class of problem already confirmed above for scan-vs-connect --
  // observed live on lissabonController with 5 dimmers: two devices
  // (striprechts, stripbank) racked up 300+ back-to-back failed attempts
  // while the others barely got a turn, which a slot-exhaustion or fairness
  // bug alone doesn't explain. If this measurably improves connect success
  // rate, make it permanent and revisit true concurrent connects later;
  // if not, revert this hunk first before looking elsewhere.
  //
  // This is only a cheap, non-atomic peek -- it lets a caller skip pointless
  // work (requestStopScanningForConnect(), log spam) when the slot is
  // obviously taken, but two callers can still both see 0 here and both
  // proceed. The actual race-free gate is tryAcquireConnectSlot(), which
  // IotsaBLEClientDevice::connect() calls immediately before attempting
  // pClient->connect() (see cwi-dis/iotsa#263) -- that compare-exchange is
  // what makes only one of them actually win.
  if (connectingCount > 0) return false;
  // Leave at least one connection slot free for the server (peripheral) role
  // if it's seen recent activity -- NimBLEDevice's client pool and
  // NimBLEServer's peer tracking share one underlying NIMBLE_MAX_CONNECTIONS
  // link budget, so unrestrained outgoing connects here can starve out an
  // incoming maintenance connection (confirmed live on lissabonController,
  // 2026-09-25, with 5 dimmers competing for 3 total slots). Only refuses a
  // *new* connect attempt -- never interrupts one already in progress.
  if (IotsaBLERadioArbiter::serverReservationActive() && NimBLEDevice::getCreatedClientCount() >= (size_t)(NIMBLE_MAX_CONNECTIONS - 1)) {
    return false;
  }
  return true;
}

void IotsaBLEClientMod::requestStopScanningForConnect() {
  // May be called from any task (e.g. BLEDimmer::connectionTask()). Do not
  // touch scanner/scanningMod here -- just flag it, loop() does the actual
  // stopScanning() call, same pattern as onScanEnd()/scanHasEnded below.
  scanStopRequested = true;
}

bool IotsaBLEClientMod::tryAcquireConnectSlot() {
  int expected = 0;
  return connectingCount.compare_exchange_strong(expected, 1);
}

void IotsaBLEClientMod::releaseConnectSlot() {
  connectingCount = 0;
}

void IotsaBLEClientMod::requestScanUpdate() {
  shouldUpdateScanAtMillis = millis();
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
  if (shouldUpdateScanAtMillis != 0 && millis() >= shouldUpdateScanAtMillis) {
    shouldUpdateScanAtMillis = 0;
    updateScanning();
  }
}

void IotsaBLEClientMod::onResult(const NimBLEAdvertisedDevice *advertisedDevice) {
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
        if (knownDeviceCallback) knownDeviceCallback(*advertisedDevice);
      }
      shouldUpdateScanAtMillis = millis(); // We may have found what we were looking for
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
    shouldUpdateScanAtMillis = millis(); // We may have found what we were looking for
    return;
  }
  if (deviceName == "") return;
  onUnknownDeviceSeen(advertisedDevice, deviceName);
}

IotsaBLEClientDevice* IotsaBLEClientMod::addDevice(std::string id, IotsaBLEClientDevice* device) {
  shouldUpdateScanAtMillis = millis(); // We probably want to scan for the new device
  auto it = devices.find(id);
  if (it == devices.end()) {
    // Device with this ID doesn't exist yet. Add it. If the caller didn't
    // hand us an already-constructed one, default to a plain
    // IotsaRunmodeBLEClient (a superset of IotsaBLEClientDevice, no
    // behavior change for existing callers that only use the base
    // interface) so every connection this mod hands out can also do the
    // generic runmode commands (identify/reboot/etc.), not just app-specific
    // get/set.
    IotsaBLEClientDevice* dev = device ? device : new IotsaRunmodeBLEClient(id);
    dev->owner = this;
    devices[id] = dev;
    return dev;
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
  IotsaBLEClientDevice *dev = addDevice(id);
  if (dev == NULL) return;
  dev->setKnownAddress(address);
  devicesByAddress[address] = dev;
  shouldUpdateScanAtMillis = millis(); // We may be able to connect right away
}

void IotsaBLEClientMod::delDevice(std::string id) {
  shouldUpdateScanAtMillis = millis();  // We may be able to stop scanning
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
#if 0
  // xxxjack bad idea to save config stright away
  saveScanConfig();
#endif
}
#endif // IOTSA_WITH_BLE
