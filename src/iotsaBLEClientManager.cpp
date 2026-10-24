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
  if (scanUnknownEnd.fired()) {
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
  scanUnknownEnd.arm(scanUnknownDurationMillis);
  iotsaController.postponeSleep(scanUnknownDurationMillis + IotsaSleepPolicy::SCAN_COMPLETION_MARGIN_MS);
}

void IotsaBLEClientManagerMod::findUnknownDevices(bool on) {
  scanForUnknownClients = on;
  scanUpdate.arm(0);
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

void IotsaBLEClientManagerMod::getHandler(IotsaApiContext& ctx) {
  IotsaBLEClientMod::getHandler(ctx);
  JsonObject reply = ctx.reply();
  if (unknownDevices.size()) {
    JsonArray unknownReply = reply["unassigned"].to<JsonArray>();
    int i = 0;
    for (auto it : unknownDevices) {
      IotsaApiContext::Scope s(ctx, "unassigned" + String(i++), JsonObject(), unknownReply.add<JsonObject>());
      it.second->getHandler(ctx);
    }
  }
  ctx.show("scanUnknown", (const char *)NULL);
  ctx.show("scanning", isScanning());
  if (!isScanning() && scanStoppedAtMillis != 0) ctx.show("lastScanSecondsAgo", (millis() - scanStoppedAtMillis) / 1000);
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
      String key(it.first.c_str());
      IotsaApiContext::Scope s(ctx, key, JsonObject(), devicesReply[key].to<JsonObject>());
      it.second->getHandler(ctx);
    }
  }
}

void IotsaBLEClientManagerMod::putHandler(IotsaApiContext& ctx) {
  IotsaBLEClientMod::putHandler(ctx);
  // Known-devices management (cwi-dis/iotsa#264): add/remove by name, or a
  // sub-object keyed by an existing device's name for that device's own
  // putHandler() (e.g. renaming, via its cwi-dis/iotsa#268 surface).
  // Managing needs "bleclient"; what a device offers itself is up to it.
  bool scanUnknown = false;
  String addName;
  bool add;
  String removeName;
  bool remove;
  {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("bleclient");
    if (ctx.arg("scan_unknown_duration", scanUnknownDurationMillis, IotsaApiContext::Saved)) saveClientConfig();
    ctx.arg("scanUnknown", scanUnknown, IotsaApiContext::Transient);
    add = ctx.arg("add", addName, IotsaApiContext::Saved);
    remove = ctx.arg("remove", removeName, IotsaApiContext::Saved);
  }
  // Snapshot first: a device's own putHandler() may call retarget(), which
  // re-keys `devices` -- mutating a std::map while iterating it directly
  // would be undefined behavior.
  std::vector<std::pair<std::string, IotsaBLEClientDevice*>> snapshot(devices.begin(), devices.end());
  for (auto& kv : snapshot) {
    String key(kv.first.c_str());
    JsonVariant devRequest = ctx.request()[key];
    if (devRequest.isNull()) continue;
    ctx.used(key.c_str());
    if (!devRequest.is<JsonObject>()) {
      ctx.error(400, key + ": expected an object");
      break;
    }
    IotsaApiContext::Scope s(ctx, key, devRequest.as<JsonObject>(), JsonObject());
    kv.second->putHandler(ctx);
  }
  // A rename: our devices are saved by name.
  if (ctx.configChanged) saveClientConfig();
  if (ctx.failed()) return;
  if (scanUnknown) startScanUnknown();
  if (add) addDeviceByName(addName.c_str());
  if (remove && !removeDeviceByName(removeName.c_str())) ctx.error(404, "remove: no such device");
}

#ifdef IOTSA_WITH_WEB
void IotsaBLEClientManagerMod::formHandler_args(IotsaWebContext& ctx) {
  // Managing needs "bleclient"; what a device offers itself is up to it.
  IotsaContext::RightsScope rights(ctx);
  ctx.argsNeedRight("bleclient");
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
      // A refused field fails the whole add.
      if (!add) return;
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
    {
      IotsaWebContext::Change remove(ctx, "remove", IotsaWebContext::Saved);
      if (remove) {
        if (removeDeviceByName(kv.first)) {
          remove.done();
        } else {
          ctx.error("Device " + String(kv.first.c_str()) + " can't be removed here.");
        }
        return;
      }
    }
    // The device's own fields start from the module's rights.
    ctx.resetRights();
    kv.second->formHandler_args(ctx);
  }
  // A rename: our devices are saved by name.
  if (ctx.configChanged) saveClientConfig();
}

void IotsaBLEClientManagerMod::formHandler_fields(IotsaWebContext& ctx) {
  String label(devicesLabel());
  // As in formHandler_args(): managing needs "bleclient", each device's own
  // fields start from the module's rights.
  if (ctx.includesSettings()) {
    for (auto& kv : devices) {
      IotsaWebContext::Scope s(ctx, String(kv.first.c_str()));
      kv.second->formHandler_fields(ctx);
      ctx.argsNeedRight("bleclient");
      ctx.showButton("remove", "Remove", IotsaWebContext::Saved);
    }
    IotsaWebContext::Scope s(ctx, "new");
    ctx.argsNeedRight("bleclient");
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
  {
    IotsaContext::RightsScope rights(ctx);
    ctx.argsNeedRight("bleclient");
    ctx.showButton("scanUnknown", "Scan for " + String(scanUnknownDurationMillis/1000) + " seconds", IotsaWebContext::Transient);
  }
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
