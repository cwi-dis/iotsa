#include "iotsaBLEClientManager.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaConfigFile.h"
#include <vector>
#include <string.h>

void IotsaBLEClientManagerMod::clientConfigLoad(IotsaConfigFileLoad& cf) {
  IotsaBLEClientMod::clientConfigLoad(cf);
  cf.get("scan_unknown_duration", scanUnknownDurationMillis, scanUnknownDurationMillis);
  collection.configLoad(cf, "");
}

void IotsaBLEClientManagerMod::clientConfigSave(IotsaConfigFileSave& cf) {
  IotsaBLEClientMod::clientConfigSave(cf);
  cf.put("scan_unknown_duration", scanUnknownDurationMillis);
  collection.configSave(cf, "");
}

bool IotsaBLEClientManagerMod::needsDiscovery() {
  return scanForUnknownClients || IotsaBLEClientMod::needsDiscovery();
}

void IotsaBLEClientManagerMod::loop() {
  IotsaBLEClientMod::loop();
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    scanUnknownUntilMillis = 0;
    findUnknownDevices(false);
  }
}

void IotsaBLEClientManagerMod::onUnknownDeviceSeen(const NimBLEAdvertisedDevice* advertisedDevice, const std::string& deviceName) {
  if (!isInterestingUnknownDevice(advertisedDevice)) {
    _noteUnrecognized(advertisedDevice, deviceName);
    return;
  }
  IotsaBLEDeviceInfo *devInfo;
  auto it3 = unknownDevices.find(deviceName);
  if (it3 == unknownDevices.end()) {
    devInfo = new IotsaBLEDeviceInfo(deviceName);
    unknownDevices[deviceName] = devInfo;
  } else {
    devInfo = it3->second;
  }
  devInfo->receivedAdvertisement(*advertisedDevice);
  if (unknownDeviceCallback) unknownDeviceCallback(*advertisedDevice);
}

void IotsaBLEClientManagerMod::onDeviceAddressChanged(IotsaBLEClientDevice* device) {
  // Ours are saved with their address: at most one flash write per device,
  // plus one whenever its address changes.
  if (collection.find(device->getName()) == device) saveClientConfig();
}

void IotsaBLEClientManagerMod::_noteUnrecognized(const NimBLEAdvertisedDevice* advertisedDevice, const std::string& deviceName) {
  const NimBLEAddress& address = advertisedDevice->getAddress();
  auto it = unrecognizedDevices.begin();
  while (it != unrecognizedDevices.end() && !(it->address == address)) ++it;
  if (it != unrecognizedDevices.end()) {
    unrecognizedDevices.splice(unrecognizedDevices.begin(), unrecognizedDevices, it);
  } else if ((int)unrecognizedDevices.size() < UNRECOGNIZED_MAX) {
    unrecognizedDevices.emplace_front();
  } else {
    unrecognizedDevices.splice(unrecognizedDevices.begin(), unrecognizedDevices, std::prev(unrecognizedDevices.end()));
  }
  UnrecognizedDevice& entry = unrecognizedDevices.front();
  entry.address = address;
  strncpy(entry.name, deviceName.c_str(), sizeof(entry.name) - 1);
  entry.name[sizeof(entry.name) - 1] = '\0';
  entry.rssi = advertisedDevice->getRSSI();
  entry.lastSeenAtMillis = millis();
}

void IotsaBLEClientManagerMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + scanUnknownDurationMillis;
  iotsaController.postponeSleep(scanUnknownDurationMillis + IotsaSleepPolicy::SCAN_COMPLETION_MARGIN_MS);
}

void IotsaBLEClientManagerMod::findUnknownDevices(bool on) {
  scanForUnknownClients = on;
  shouldUpdateScanAtMillis = millis();
}

bool IotsaBLEClientManagerMod::addDeviceByName(const std::string& name) {
  if (collection.add(name) == nullptr) return false;
  saveClientConfig();
  return true;
}

bool IotsaBLEClientManagerMod::removeDeviceByName(const std::string& name) {
  if (!collection.remove(name)) return false;
  saveClientConfig();
  return true;
}

void IotsaBLEClientManagerMod::setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback) {
  unknownDeviceCallback = _callback;
}

bool IotsaBLEClientManagerMod::getHandler(const char *path, JsonObject& reply) {
  bool rv = IotsaBLEClientMod::getHandler(path, reply);
  if (unknownDevices.size()) {
    JsonArray unknownReply = reply["unassigned"].to<JsonArray>();
    for (auto it : unknownDevices) {
      JsonObject devReply = unknownReply.add<JsonObject>();
      it.second->getHandler(devReply);
    }
  }
  reply["scanUnknown"] = (char *)NULL;
  reply["scanning"] = isScanning();
  if (!isScanning() && scanStoppedAtMillis != 0) reply["lastScanSecondsAgo"] = (millis() - scanStoppedAtMillis) / 1000;
  if (unrecognizedDevices.size()) {
    JsonArray unrecognizedReply = reply["unrecognized"].to<JsonArray>();
    uint32_t now = millis();
    for (auto& entry : unrecognizedDevices) {
      JsonObject e = unrecognizedReply.add<JsonObject>();
      e["address"] = entry.address.toString();
      e["name"] = entry.name;
      e["lastSeenSecondsAgo"] = (now - entry.lastSeenAtMillis) / 1000;
      e["rssi"] = entry.rssi;
    }
  }
  // Known devices (cwi-dis/iotsa#264), each reported via its own getHandler()
  // -- name/address/found/connected/connect-stats, from cwi-dis/iotsa#268.
  if (devices.size()) {
    JsonObject devicesReply = reply["devices"].to<JsonObject>();
    for (auto it : devices) {
      JsonObject devReply = devicesReply[String(it.first.c_str())].to<JsonObject>();
      it.second->getHandler(devReply);
    }
  }
  return rv;
}

bool IotsaBLEClientManagerMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (!IotsaBLEClientMod::putHandler(path, request, reply, changed)) return false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<int>(reqObj, "scan_unknown_duration", scanUnknownDurationMillis)) {
    saveClientConfig();
    if (changed) *changed = true;
  }
  bool scanUnknown;
  if (getFromRequest<bool>(reqObj, "scanUnknown", scanUnknown) && scanUnknown) {
    startScanUnknown();
  }
  // Known-devices management (cwi-dis/iotsa#264): add/remove by name, or
  // route a sub-object keyed by an existing device's name to that device's
  // own putHandler() (e.g. renaming, via its cwi-dis/iotsa#268 surface).
  String addName;
  if (getFromRequest<String>(reqObj, "add", addName) && addName != "") {
    if (addDeviceByName(addName.c_str()) && changed) *changed = true;
  }
  String removeName;
  if (getFromRequest<String>(reqObj, "remove", removeName) && removeName != "") {
    if (!removeDeviceByName(removeName.c_str())) return apiError(404, "remove: no such device");
    if (changed) *changed = true;
  }
  // Snapshot first: a device's own putHandler() may call retarget(), which
  // re-keys `devices` -- mutating a std::map while iterating it directly
  // would be undefined behavior.
  std::vector<std::pair<std::string, IotsaBLEClientDevice*>> snapshot(devices.begin(), devices.end());
  bool deviceChanged = false;
  for (auto& kv : snapshot) {
    String key(kv.first.c_str());
    JsonVariant devRequest = reqObj[key];
    if (devRequest.isNull()) continue;
    if (!kv.second->putHandler(devRequest, reply, &deviceChanged)) return false;
    reqObj.remove(key);
  }
  if (deviceChanged) {
    // A rename: our devices are saved by name.
    saveClientConfig();
    if (changed) *changed = true;
  }
  checkUnhandled(reqObj);
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaBLEClientManagerMod::webHandler() {
  bool anyChanged = false;
  anyChanged |= formHandler_args(api.webService->server, "", true);
  if (anyChanged) saveClientConfig();
  String message = "<html><head><title>BLE Devices</title></head><body><h1>BLE Devices</h1>";

  formHandler_fields(message, devicesLabel(), "bledevice", true);

  message += "<form method='get'><input type='submit' name='refresh' value='Refresh'></form>";
  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
}

void IotsaBLEClientManagerMod::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  // Known-devices listing (cwi-dis/iotsa#264): generalizes what lissabon's
  // DimmerCollection/DimmerDynamicCollection used to build for itself --
  // each device already knows how to render its own name/found/connected
  // status and (if includeConfig) an editable name field, via its own
  // IotsaApiModObject surface (cwi-dis/iotsa#268). This mod just lists them.
  message += "<h2>Known " + text + "</h2>";
  if (devices.size() == 0) {
    message += "<p>No known " + text + " yet.</p>";
  } else {
    for (auto it : devices) {
      String name(it.first.c_str());
      it.second->formHandler_fields(message, name + ": ", name, includeConfig);
      if (includeConfig) {
        message += "<form method='get'><input type='hidden' name='remove' value='" + name + "'><input type='submit' value='Remove'></form>";
      }
    }
  }
  if (includeConfig) {
    message += "<form method='get'>Add device by name: <input name='add'><input type='submit' value='Add'></form>";
  }
  message += "<h2>Unknown " + text + "</h2>";
  message += "<form method='get'><input type='submit' name='scanUnknown' value='Scan for " + String(scanUnknownDurationMillis/1000) + " seconds'></form>";
  message += "<form method='get'><input type='submit' name='refresh' value='Refresh'></form>";
  if (unknownDevices.size() == 0) {
    message += "<p>No unknown " + text + " seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDevices) {
      message += "<li>" + formHandler_field_perdevice(it.first.c_str()) + " (RSSI " + String(it.second->getRSSI()) + ")</li>";
    }
    message += "</ul>";
  }
  message += "<h2>Recent unrecognized BLE devices</h2>";
  if (unrecognizedDevices.size() == 0) {
    message += "<p>None seen recently.</p>";
  } else {
    message += "<table><tr><th>Address</th><th>Name</th><th>Seconds ago</th><th>RSSI</th></tr>";
    uint32_t now = millis();
    for (auto& entry : unrecognizedDevices) {
      message += "<tr><td>" + String(entry.address.toString().c_str()) + "</td><td>" + htmlEncode(entry.name) + "</td><td>" + String((now - entry.lastSeenAtMillis) / 1000) + "</td><td>" + String(entry.rssi) + "</td></tr>";
    }
    message += "</table>";
  }
}

String IotsaBLEClientManagerMod::formHandler_field_perdevice(const char *deviceName) {
  // Advertised names come from anyone nearby.
  return htmlEncode(deviceName);
}

bool IotsaBLEClientManagerMod::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool anyChanged = false;
  if (server->hasArg("scanUnknown")) startScanUnknown();
  if (includeConfig && server->hasArg("add")) {
    String addName = server->arg("add");
    if (addName != "" && addDeviceByName(addName.c_str())) anyChanged = true;
  }
  if (includeConfig && server->hasArg("remove")) {
    String removeName = server->arg("remove");
    if (removeName != "" && removeDeviceByName(removeName.c_str())) anyChanged = true;
  }
  // Snapshot first: a device's own formHandler_args() may call retarget(),
  // which re-keys `devices` -- mutating a std::map while iterating it
  // directly would be undefined behavior.
  std::vector<std::pair<std::string, IotsaBLEClientDevice*>> snapshot(devices.begin(), devices.end());
  for (auto& kv : snapshot) {
    String name(kv.first.c_str());
    if (kv.second->formHandler_args(server, name, includeConfig)) anyChanged = true;
  }
  return anyChanged;
}
#endif // IOTSA_WITH_WEB

#endif // IOTSA_WITH_BLE
