#include "iotsaBLEClientCollection.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaConfigFile.h"
#include <vector>

void IotsaBLEClientCollectionMod::loadScanConfig() {
  IotsaBLEClientMod::loadScanConfig();
  IotsaConfigFileLoad cf("/config/bleclient.cfg");
  cf.get("scan_unknown_duration", scanUnknownDurationMillis, scanUnknownDurationMillis);
}

void IotsaBLEClientCollectionMod::saveScanConfig() {
  IotsaBLEClientMod::saveScanConfig();
  IotsaConfigFileSave cf("/config/bleclient.cfg");
  cf.put("scan_unknown_duration", scanUnknownDurationMillis);
}

bool IotsaBLEClientCollectionMod::needsDiscovery() {
  return scanForUnknownClients || IotsaBLEClientMod::needsDiscovery();
}

void IotsaBLEClientCollectionMod::loop() {
  IotsaBLEClientMod::loop();
  if (scanUnknownUntilMillis != 0 && millis() > scanUnknownUntilMillis) {
    scanUnknownUntilMillis = 0;
    findUnknownDevices(false);
  }
}

void IotsaBLEClientCollectionMod::onUnknownDeviceSeen(const NimBLEAdvertisedDevice* advertisedDevice, const std::string& deviceName) {
  if (!isInterestingUnknownDevice(advertisedDevice)) return;
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

void IotsaBLEClientCollectionMod::startScanUnknown() {
  findUnknownDevices(true);
  scanUnknownUntilMillis = millis() + scanUnknownDurationMillis;
  iotsaController.postponeSleep(scanUnknownDurationMillis + IotsaSleepPolicy::SCAN_COMPLETION_MARGIN_MS);
}

void IotsaBLEClientCollectionMod::findUnknownDevices(bool on) {
  scanForUnknownClients = on;
  shouldUpdateScanAtMillis = millis();
}

bool IotsaBLEClientCollectionMod::addDeviceByName(const std::string& name) {
  if (getDevice(name) != nullptr) return false;
  addDevice(name);
  return true;
}

bool IotsaBLEClientCollectionMod::removeDeviceByName(const std::string& name) {
  if (getDevice(name) == nullptr) return false;
  delDevice(name);
  return true;
}

void IotsaBLEClientCollectionMod::setUnknownDeviceFoundCallback(BleDeviceFoundCallback _callback) {
  unknownDeviceCallback = _callback;
}

bool IotsaBLEClientCollectionMod::getHandler(const char *path, JsonObject& reply) {
  bool rv = IotsaBLEClientMod::getHandler(path, reply);
  if (unknownDevices.size()) {
    JsonArray unknownReply = reply["unassigned"].to<JsonArray>();
    for (auto it : unknownDevices) {
      JsonObject devReply = unknownReply.add<JsonObject>();
      it.second->getHandler(devReply);
    }
  }
  reply["scanUnknown"] = (char *)NULL;
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

bool IotsaBLEClientCollectionMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  if (!IotsaBLEClientMod::putHandler(path, request, reply)) return false;
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<int>(reqObj, "scan_unknown_duration", scanUnknownDurationMillis)) {
    saveScanConfig();
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
    addDeviceByName(addName.c_str());
  }
  String removeName;
  if (getFromRequest<String>(reqObj, "remove", removeName) && removeName != "") {
    if (!removeDeviceByName(removeName.c_str())) return apiError(404, "remove: no such device");
  }
  // Snapshot first: a device's own putHandler() may call retarget(), which
  // re-keys `devices` -- mutating a std::map while iterating it directly
  // would be undefined behavior.
  std::vector<std::pair<std::string, IotsaBLEClientDevice*>> snapshot(devices.begin(), devices.end());
  for (auto& kv : snapshot) {
    String key(kv.first.c_str());
    JsonVariant devRequest = reqObj[key];
    if (devRequest.isNull()) continue;
    if (!kv.second->putHandler(devRequest, reply)) return false;
    reqObj.remove(key);
  }
  checkUnhandled(reqObj);
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaBLEClientCollectionMod::webHandler() {
  bool anyChanged = false;
  anyChanged |= formHandler_args(api.webService->server, "", true);
  if (anyChanged) saveScanConfig();
  String message = "<html><head><title>BLE Devices</title></head><body><h1>BLE Devices</h1>";

  formHandler_fields(message, "BLE devices", "bledevice", true);

  message += "<form method='get'><input type='submit' name='refresh' value='Refresh'></form>";
  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
}

void IotsaBLEClientCollectionMod::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  // Known-devices listing (cwi-dis/iotsa#264): generalizes what lissabon's
  // DimmerCollection/DimmerDynamicCollection used to build for itself --
  // each device already knows how to render its own name/found/connected
  // status and (if includeConfig) an editable name field, via its own
  // IotsaApiModObject surface (cwi-dis/iotsa#268). This mod just lists them.
  message += "<h2>Known " + text + " devices</h2>";
  if (devices.size() == 0) {
    message += "<p>No known devices yet.</p>";
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
  message += "<h2>Available Unknown/new " + text + " devices</h2>";
  message += "<form method='get'><input type='submit' name='scanUnknown' value='Scan for " + String(scanUnknownDurationMillis/1000) + " seconds'></form>";
  message += "<form method='get'><input type='submit' name='refresh' value='Refresh'></form>";
  if (unknownDevices.size() == 0) {
    message += "<p>No unassigned BLE dimmer devices seen recently.</p>";
  } else {
    message += "<ul>";
    for (auto it: unknownDevices) {
      message += "<li>" + formHandler_field_perdevice(it.first.c_str()) + " (RSSI " + String(it.second->getRSSI()) + ")</li>";
    }
    message += "</ul>";
  }
}

String IotsaBLEClientCollectionMod::formHandler_field_perdevice(const char *deviceName) {
  return String(deviceName);
}

bool IotsaBLEClientCollectionMod::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
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
