#include "iotsa.h"
#include "iotsaBLEClient.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaConfigFile.h"
#include "iotsaBLEServer.h"

//
// IotsaBLEClientMod is intended to be used as a base class
// for other modules (which will then save configurations, etc, for the
// devices the module is interested in).
//

const int SCAN_START_RETRY_MS = 1000; // How long to wait before retrying start scan
const int SCAN_UNKNOWN_DURATION_MS = 20000; // How long to scan for unknown clients

bool IotsaBLEClientMod::coordinateWithServer = false;

void IotsaBLEClientMod::configLoad() {
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  cf.get("scan_interval", scan_interval, scan_interval);
  cf.get("scan_window", scan_window, scan_window);
  cf.get("scan_duration_discovery", scanDurationDiscoveryMillis, scanDurationDiscoveryMillis);
  cf.get("scan_duration_presence", scanDurationPresenceMillis, scanDurationPresenceMillis);
  cf.get("scan_cooldown_discovery", scanCooldownDiscoveryMillis, scanCooldownDiscoveryMillis);
  cf.get("scan_cooldown_presence", scanCooldownPresenceMillis, scanCooldownPresenceMillis);
  cf.get("connect_settle_time", connectSettleTimeMillis, connectSettleTimeMillis);
  cf.get("connect_timeout", connectTimeoutMillis, connectTimeoutMillis);
}

void IotsaBLEClientMod::configSave() {
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  cf.put("scan_interval", scan_interval);
  cf.put("scan_window", scan_window);
  cf.put("scan_duration_discovery", scanDurationDiscoveryMillis);
  cf.put("scan_duration_presence", scanDurationPresenceMillis);
  cf.put("scan_cooldown_discovery", scanCooldownDiscoveryMillis);
  cf.put("scan_cooldown_presence", scanCooldownPresenceMillis);
  cf.put("connect_settle_time", connectSettleTimeMillis);
  cf.put("connect_timeout", connectTimeoutMillis);
}

void IotsaBLEClientMod::setup() {
  IFDEBUG IotsaSerial.println("BLEClientmod::setup()");
  configLoad();
  iotsaBLE_ensureInitialized();
  setupScanner();
}

void IotsaBLEClientMod::setupScanner() {
  // The scanner is a singleton. We initialize it once.
  scanner = BLEDevice::getScan();
  scanner->setScanCallbacks(this, false);
  scanner->setActiveScan(true);
  scanner->setInterval(scan_interval);
  scanner->setWindow(scan_window);
  scanner = NULL;

}

#ifdef IOTSA_WITH_WEB
void
IotsaBLEClientMod::handler() {
  bool anyChanged = false;
  anyChanged |= formHandler_args(server, "", true);
  if (anyChanged) configSave();
  String message = "<html><head><title>BLE Devices</title></head><body><h1>BLE Devices</h1>";

  formHandler_fields(message, "BLE devices", "bledevice", true);

  message += "<form method='post'><input type='submit' name='refresh' value='Refresh'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

void IotsaBLEClientMod::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  message += "<h2>Available Unknown/new " + text + " devices</h2>";
  message += "<form method='post'><input type='submit' name='scanUnknown' value='Scan for 20 seconds'></form>";
  message += "<form method='post'><input type='submit' name='refresh' value='Refresh'></form>";
  if (unknownDevices.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDevices) {
      message += "<li>" + formHandler_field_perdevice(it.c_str()) + "</li>";
    }
    message += "</ul>";
  }
}

String IotsaBLEClientMod::formHandler_field_perdevice(const char *deviceName) {
  return String(deviceName);
}

bool IotsaBLEClientMod::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  if (server->hasArg("scanUnknown")) startScanUnknown();
  return false;
}

#endif // IOTSA_WITH_WEB

bool IotsaBLEClientMod::getHandler(const char *path, JsonObject& reply) {
  reply["scan_interval"] = scan_interval;
  reply["scan_window"] = scan_window;
  reply["scan_duration_discovery"] = scanDurationDiscoveryMillis;
  reply["scan_duration_presence"] = scanDurationPresenceMillis;
  reply["scan_cooldown_discovery"] = scanCooldownDiscoveryMillis;
  reply["scan_cooldown_presence"] = scanCooldownPresenceMillis;
  reply["connect_settle_time"] = connectSettleTimeMillis;
  reply["connect_timeout"] = connectTimeoutMillis;
  if (unknownDevices.size()) {
    JsonArray unknownReply = reply["unassigned"].as<JsonArray>();
    for (auto it : unknownDevices) {
      unknownReply.add((char *)it.c_str());
    }
  }
  reply["scanUnknown"] = (char *)NULL;
  return true;
}
bool IotsaBLEClientMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool _startScanUnknown = false;
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
  if (getFromRequest<int>(reqObj, "scan_duration_presence", scanDurationPresenceMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "scan_cooldown_discovery", scanCooldownDiscoveryMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "scan_cooldown_presence", scanCooldownPresenceMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "connect_settle_time", connectSettleTimeMillis)) anyChanged = true;
  if (getFromRequest<int>(reqObj, "connect_timeout", connectTimeoutMillis)) anyChanged = true;
  if (reqObj["scanUnknown"]|0) {
    _startScanUnknown = true;
  }
  if (anyChanged) {
    configSave();
    setupScanner();
  }
  if (_startScanUnknown) {
    startScanUnknown();
  }
  return anyChanged;
}

void IotsaBLEClientMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + SCAN_UNKNOWN_DURATION_MS;
  iotsaConfig.postponeSleep(SCAN_UNKNOWN_DURATION_MS+1000);
}

void IotsaBLEClientMod::findUnknownDevices(bool on) {
  scanForUnknownClients = on;
  shouldUpdateScanAtMillis = millis();
}

bool IotsaBLEClientMod::isScanning() {
  return scanner != nullptr && scanner->isScanning();
}

unsigned int IotsaBLEClientMod::maxConnectionKeepOpen() {
  int rv = 30000; // Random large value
  if (shouldUpdateScanAtMillis != 0) {
    int rv2 = shouldUpdateScanAtMillis - millis();
    if (rv2 < 0) rv2 = 0;
    rv = rv2;
  }
  return rv;
}

bool IotsaBLEClientMod::needsDiscovery() {
  // We need active discovery if we're hunting for unknown devices, or any
  // known device has never been matched by name yet (no address at all).
  if (scanForUnknownClients) return true;
  for (auto it: devices) {
    if (!it.second->available()) return true;
  }
  return false;
}

bool IotsaBLEClientMod::allKnownDevicesSeenSince(uint32_t sinceMillis) {
  for (auto it: devices) {
    IotsaBLEClientConnection* dev = it.second;
    if (!dev->available()) continue; // no address yet, discovery's job, not presence-check's
    if (dev->isConnected()) continue; // an active connection is proof enough of presence
    if ((int32_t)(dev->getLastSeenAtMillis() - sinceMillis) < 0) return false;
  }
  return true;
}

void IotsaBLEClientMod::updateScanning() {
  if (isScanning()) {
    // A presence-check scan (as opposed to a discovery scan) stops as soon as
    // it's confirmed everyone's still there, rather than running its full
    // duration -- the whole point is a quick, infrequent liveness check.
    if (!currentScanIsDiscovery && allKnownDevicesSeenSince(scanStartedAtMillis)) {
      stopScanning();
    }
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
  // Nothing to scan for at all: no known devices, and not hunting for unknowns.
  if (devices.empty() && !scanForUnknownClients) return;
  IFDEBUG {
    IotsaSerial.print("BLE scan for: ");
    if (scanForUnknownClients) {
      IotsaSerial.print("(new/unknown) ");
    }
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
#if 0
  // First close all connections. Scanning while connected has proved to result in issues.
  for (auto it : devices) {
    if (it.second && it.second->isConnected()) {
      if (!disconnectClientsForScan) {
        // Don't scan if any active clients. But next time around we will disconnect them
        IFDEBUG IotsaSerial.println("BLE scan aborted: active connection");
        shouldUpdateScan = true;
        dontUpdateScanBefore = millis() + PAUSE_BETWEEN_SCANS;
        disconnectClientsForScan = true;
        return;
      }
      IFDEBUG IotsaSerial.printf("BLE scan start: disconnect %s\n", it.second->name.c_str());
      it.second->disconnect();
    }
  }
#endif
  if (coordinateWithServer) {
    advertisingWasPausedByScan = IotsaBLEServerMod::pauseServer();
  }
  // Now start the scan
  currentScanIsDiscovery = needsDiscovery();
  uint32_t duration = currentScanIsDiscovery ? scanDurationDiscoveryMillis : scanDurationPresenceMillis;
  scanner = BLEDevice::getScan();
  scanningMod = this;
  scanStartedAtMillis = millis();
  bool startOk = scanner->start(duration);
  iotsaBLE_notifyScanningStateChanged(startOk && scanner->isScanning());
  if (!startOk) {
    scanner = nullptr;
    IFDEBUG IotsaSerial.println("BLEClient: cannot start scan, retry in 1s");
    shouldUpdateScanAtMillis = millis() + SCAN_START_RETRY_MS;
    return;
  }
  scanningChanged();
#if 0
  // Do not sleep until scan is done
  iotsaConfig.pauseSleep();
#endif
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
    // We can sleep again, but give a bit of time to cient-connection objects to
    // react to scan results.
  #if 0
    iotsaConfig.resumeSleep();
    iotsaConfig.postponeSleep(100);
  #endif
    scanningChanged();
  }
  // Next time through loop, check whether we should scan again -- discovery
  // and presence-check scans get their own configurable cooldown.
  shouldUpdateScanAtMillis = millis() + (currentScanIsDiscovery ? scanCooldownDiscoveryMillis : scanCooldownPresenceMillis);
}

bool IotsaBLEClientMod::canConnect() {
  // Connecting to a device while we are scanning has proved to result in issues
  // (confirmed live 2026-07-18: NimBLE's ble_gap_connect() outright rejects a
  // connection attempt while a scan is active). Also require a short settle
  // time after scanning stops -- an immediate connect right after stopScanning()
  // has also been observed to fail.
  if (scanner != NULL) return false;
  if (millis() - scanStoppedAtMillis < connectSettleTimeMillis) return false;
  return true;
}

void IotsaBLEClientMod::requestStopScanningForConnect() {
  // May be called from any task (e.g. BLEDimmer::connectionTask()). Do not
  // touch scanner/scanningMod here -- just flag it, loop() does the actual
  // stopScanning() call, same pattern as onScanEnd()/scanHasEnded below.
  scanStopRequested = true;
}

void IotsaBLEClientMod::noteConnectAttemptStarted() {
  connectingCount++;
}

void IotsaBLEClientMod::noteConnectAttemptEnded() {
  connectingCount--;
}

IotsaBLEClientMod* IotsaBLEClientMod::scanningMod = NULL;

void IotsaBLEClientMod::onScanEnd(const NimBLEScanResults& scanResults, int reason) {
    // Called on the NimBLE host task, not the main loop() task. Do not touch
    // scanner/scanningMod or anything else non-trivial here -- just flag it
    // and let loop() (single-threaded) do the actual work.
    IFDEBUG IotsaSerial.printf("IotsaBLEClientMod: BLE scan complete, reason=%d\n", reason);
    scanHasEnded = true;
}

void IotsaBLEClientMod::serverSetup() {
  api.setup("/api/bleclient", true, true, false);
  name = "bleclient";
#ifdef IOTSA_WITH_WEB
  server->on("/bleclient", std::bind(&IotsaBLEClientMod::handler, this));
#endif
}

void IotsaBLEClientMod::setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback) {
  unknownDeviceCallback = _callback;
}

void IotsaBLEClientMod::setKnownDeviceChangedCallback(BleDeviceFoundCallback _callback) {
  knownDeviceCallback = _callback;
}

void IotsaBLEClientMod::setDuplicateNameFilter(bool noDuplicateNames) {
  duplicateNameFilter = noDuplicateNames;
}

void IotsaBLEClientMod::setServiceFilter(const BLEUUID& serviceUUID) {
  if (serviceFilter) delete serviceFilter;
  serviceFilter = new BLEUUID(serviceUUID);
}

void IotsaBLEClientMod::setManufacturerFilter(uint16_t manufacturerID) {
  manufacturerFilter = manufacturerID;
  hasManufacturerFilter = true;
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
    iotsaConfig.resumeSleep();
    wantStop = true;
  } else if (scanner != nullptr && !scanner->isScanning()) {
    // Fallback in case onScanEnd() is ever missed.
    wantStop = true;
  }
  if (wantStop && scanner != nullptr) {
    stopScanning();
  }
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    scanUnknownUntilMillis = 0;
    findUnknownDevices(false);
  }
  if (shouldUpdateScanAtMillis != 0 && millis() >= shouldUpdateScanAtMillis) {
    shouldUpdateScanAtMillis = 0;
    updateScanning();
  }
}

void IotsaBLEClientMod::onResult(const BLEAdvertisedDevice *advertisedDevice) {
#ifdef DEBUG_PRINT_ALL_CLIENTS
  IotsaSerial.printf("BLEClientMod::onResult(%s)\n", advertisedDevice->toString().c_str());
#endif
  // Is this an advertisement for a device we know, either by name or by address?
  // (NimBLE-Arduino 2.1.0 stopped advertising the device name by default, so a
  // known device may well show up with no name at all -- the address match below
  // has to be reachable even then.)
  std::string deviceName = advertisedDevice->getName();
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
        knownDeviceCallback(*advertisedDevice);
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
      knownDeviceCallback(*advertisedDevice);
    }
    shouldUpdateScanAtMillis = millis(); // We may have found what we were looking for
    return;
  }
  // Do we want callbacks for unknown devices?
  if (unknownDeviceCallback == NULL) return;
  if (deviceName == "") return;
  // Have we seen this unknown device before?
  if ( duplicateNameFilter && unknownDevices.find(deviceName) != unknownDevices.end()) return;
  // Do we filter on services?
  if (serviceFilter != NULL) {
    if (!advertisedDevice->isAdvertisingService(*serviceFilter)) return;
  }
  // Do we filter on manufacturer data?
  if (hasManufacturerFilter) {
    std::string mfgData(advertisedDevice->getManufacturerData());
    if (mfgData.length() < 2) return;
    const uint16_t *mfg = (const uint16_t *)mfgData.c_str();
    if (*mfg != manufacturerFilter) return;
  }
  unknownDeviceCallback(*advertisedDevice);
}

IotsaBLEClientConnection* IotsaBLEClientMod::addDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    // Device with this ID doesn't exist yet. Add it.
    IotsaBLEClientConnection* dev = new IotsaBLEClientConnection(id);
    dev->owner = this;
    devices[id] = dev;
    return dev;
  }
  shouldUpdateScanAtMillis = millis(); // We probably want to scan for the new device
  return it->second;
}

IotsaBLEClientConnection* IotsaBLEClientMod::getDevice(std::string id) {
  auto it = devices.find(id);
  if (it == devices.end()) {
    return NULL;
  }
  return it->second;
}

void IotsaBLEClientMod::deviceNotConnectable(std::string id) {
  IotsaBLEClientConnection *dev;
  dev = addDevice(id);
  if (dev == NULL) return;
  dev->clearDevice();
  shouldUpdateScanAtMillis = millis(); // We may want to start scanning again
}

void IotsaBLEClientMod::noteKnownAddress(std::string id, std::string address) {
  if (address == "") return;
  IotsaBLEClientConnection *dev = addDevice(id);
  if (dev == NULL) return;
  dev->setKnownAddress(address);
  devicesByAddress[address] = dev;
  shouldUpdateScanAtMillis = millis(); // We may be able to connect right away
}

void IotsaBLEClientMod::delDevice(std::string id) {
  shouldUpdateScanAtMillis = millis();  // We may be able to stop scanning
  int nDeleted = devices.erase(id);
#if 0
  // xxxjack bad idea to save config stright away
  if (nDeleted > 0) configSave();
#endif
}
#endif // IOTSA_WITH_BLE
