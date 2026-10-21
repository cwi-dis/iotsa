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
void IotsaBLEClientManagerMod::formHandler_args(IotsaWebContext& ctx) {
  if (ctx.argAction("scanUnknown", IotsaWebContext::Transient)) {
    startScanUnknown();
    ctx.feedback("Scanning for " + String(scanUnknownDurationMillis/1000) + " seconds.");
  }
  {
    IotsaWebContext::Scope s(ctx, "new");
    IotsaWebContext::Change add(ctx, "add", IotsaWebContext::Saved);
    if (add) {
      String name;
      ctx.arg("name", name, IotsaWebContext::Saved);
      if (name == "") {
        ctx.error("A new device needs a name.");
      } else if (getDevice(name) != nullptr) {
        ctx.error("Device " + name + " already exists.");
      } else if (addDeviceByName(name.c_str())) {
        add.done();
      } else {
        ctx.error("Device " + name + " can't be added here.");
      }
      return;
    }
  }
  // Snapshot first: a device's own formHandler_args() may call retarget(),
  // which re-keys `devices` -- mutating a std::map while iterating it
  // directly would be undefined behavior.
  std::vector<std::pair<std::string, IotsaBLEClientDevice*>> snapshot(devices.begin(), devices.end());
  for (auto& kv : snapshot) {
    IotsaWebContext::Scope s(ctx, String(kv.first.c_str()));
    IotsaWebContext::Change remove(ctx, "remove", IotsaWebContext::Saved);
    if (remove) {
      if (removeDeviceByName(kv.first)) {
        remove.done();
      } else {
        ctx.error("Device " + String(kv.first.c_str()) + " can't be removed here.");
      }
      return;
    }
    kv.second->formHandler_args(ctx);
  }
  // A rename: our devices are saved by name.
  if (ctx.configChanged) saveClientConfig();
}

void IotsaBLEClientManagerMod::formHandler_fields(IotsaWebContext& ctx) {
  String label(devicesLabel());
  if (ctx.includesSettings()) {
    for (auto& kv : devices) {
      IotsaWebContext::Scope s(ctx, String(kv.first.c_str()));
      kv.second->formHandler_fields(ctx);
      ctx.showButton("remove", "Remove", IotsaWebContext::Saved);
    }
    IotsaWebContext::Scope s(ctx, "new");
    IotsaWebContext::Form form(ctx, "Add", "Add a device", "add");
    ctx.showField("BLE device name", "name", "", IotsaWebContext::Saved);
    return;
  }
  // Known-devices listing (cwi-dis/iotsa#264): each device renders its own
  // status (cwi-dis/iotsa#268), this module just lists them.
  ctx << "<h3>Known " + htmlEncode(label) + "</h3>\n";
  if (devices.size() == 0) {
    ctx << "<p>No known " + htmlEncode(label) + " yet.</p>\n";
  }
  for (auto& kv : devices) {
    IotsaWebContext::Scope s(ctx, String(kv.first.c_str()));
    kv.second->formHandler_fields(ctx);
  }
  ctx << "<h3>Unknown " + htmlEncode(label) + "</h3>\n";
  ctx.showButton("scanUnknown", "Scan for " + String(scanUnknownDurationMillis/1000) + " seconds", IotsaWebContext::Transient);
  if (unknownDevices.size() == 0) {
    ctx << "<p>No unknown " + htmlEncode(label) + " seen recently.</p>\n";
  } else {
    ctx << "<ul>\n";
    for (auto it: unknownDevices) {
      ctx << "<li>" + formHandler_field_perdevice(it.first.c_str()) + " (RSSI " + String(it.second->getRSSI()) + ")</li>\n";
    }
    ctx << "</ul>\n";
  }
  ctx << "<h3>Recent unrecognized BLE devices</h3>\n";
  if (unrecognizedDevices.size() == 0) {
    ctx << "<p>None seen recently.</p>\n";
  } else {
    ctx << "<table><tr><th>Address</th><th>Name</th><th>Seconds ago</th><th>RSSI</th></tr>\n";
    uint32_t now = millis();
    for (auto& entry : unrecognizedDevices) {
      ctx << "<tr><td>" + String(entry.address.toString().c_str()) + "</td><td>" + htmlEncode(entry.name) + "</td><td>" + String((now - entry.lastSeenAtMillis) / 1000) + "</td><td>" + String(entry.rssi) + "</td></tr>\n";
    }
    ctx << "</table>\n";
  }
}

String IotsaBLEClientManagerMod::formHandler_field_perdevice(const char *deviceName) {
  // Advertised names come from anyone nearby.
  return htmlEncode(deviceName);
}
#endif // IOTSA_WITH_WEB

#endif // IOTSA_WITH_BLE
