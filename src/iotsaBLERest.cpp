#include "iotsa.h"
#include "iotsaBLEREST.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_WEB

String IotsaBLERestMod::info() {
  String message = "<p>Built with support for REST over Bluetooth LE.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaBLERestMod::getHandler(const char *path, JsonObject& reply) {
    return true;
}

bool IotsaBLERestMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
    return false;
}

bool IotsaBLERestMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == commandUUID) {
    curCommand = bleApi.getAsString(charUUID);
    IotsaSerial.printf("IotsaBLERestMod: command=%s\n", curCommand.c_str());
    _processRequest();
    return true;
  }
  if (charUUID == dataUUID) {
    curData += bleApi.getAsString(charUUID);
    IotsaSerial.printf("IotsaBLERestMod: data=%s\n", curData.c_str());
    return true;
  }
  IotsaSerial.println("IotsaBLERestMod: ble: write unknown uuid");
  return false;
}

bool IotsaBLERestMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == responseUUID) {
    IotsaSerial.printf("IotsaBLERestMod: response=%s\n", curResponse.c_str());
    bleApi.set(charUUID, curResponse);
    curResponse = "";
    return true;
  }
  IotsaSerial.println("IotsaBLERestMod: ble: read unknown uuid");
  return false;
}

void IotsaBLERestMod::_processRequest() {
    if (curCommand == "") return;
    curResponse = curData;
    curData = "";
    curCommand = "";
}

void IotsaBLERestMod::setup() {
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(commandUUID, BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "BLE REST Command");
  bleApi.addCharacteristic(dataUUID, BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "BLE REST Data");
  bleApi.addCharacteristic(responseUUID, BLE_READ|BLE_NOTIFY, BLE2904::FORMAT_UTF8, 0x2700, "BLE REST Response");
}

void IotsaBLERestMod::serverSetup() {
  api.setup("/api/blerest", true, false);
  name = "blerest";
}

void IotsaBLERestMod::loop() {
}
