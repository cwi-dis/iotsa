#include "iotsa.h"
#include "iotsaBLERest.h"
#include "iotsaBLERestApi.h"
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
    // xxxjack doesn't handle MTU yet
    std::string tmp = curResponse;
    curResponse = "";
    IotsaSerial.printf("IotsaBLERestMod: response=%s\n", tmp.c_str());
    bleApi.set(charUUID, tmp);
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

std::list<IotsaBLERestApiService*> IotsaBLERestApiService::all;

IotsaBLERestApiService::IotsaBLERestApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
: provider(_provider),
  auth(_auth)
{
  all.push_back(this);
}
  
void IotsaBLERestApiService::setup(const char* path, bool get, bool put, bool post) {
  IotsaSerial.printf("IotsaBLERestApiService: path=%s\n", path);
  provider_path = path;
  provider_get = get;
  provider_put = put;
  provider_post = post;
}