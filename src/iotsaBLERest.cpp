#include "iotsa.h"
#include "iotsaBLERest.h"
#include "iotsaBLERestApi.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif

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
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: command=%s\n", curCommand.c_str());
    _processRequest();
    return true;
  }
  if (charUUID == dataUUID) {
    curData += bleApi.getAsString(charUUID);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: data=%s\n", curData.c_str());
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
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: response=%s\n", tmp.c_str());
    bleApi.set(charUUID, tmp);
    return true;
  }
  IotsaSerial.println("IotsaBLERestMod: ble: read unknown uuid");
  return false;
}

int IotsaBLERestMod::_processRequest() {
  curResponse = "";
  if (curCommand == "") {
    IotsaSerial.println("IotsaBLERest: empty command");
    return 400;
  }
  IFDEBUG IotsaSerial.print("BLEREST ");
  IFDEBUG IotsaSerial.println(curCommand.c_str());
  bool cmd_get = false;
  bool cmd_put = false;
  bool cmd_post = false;
  std::string url;
  if (curCommand.rfind("GET ", 0) == 0) {
    cmd_get = true;
    url = curCommand.substr(4);
  } else
  if (curCommand.rfind("PUT ", 0) == 0) {
    cmd_put = true;
    url = curCommand.substr(4);
  } else
  if (curCommand.rfind("POST ", 0) == 0) {
    cmd_post = true;
    url = curCommand.substr(5);
  } else {
    IotsaSerial.printf("IotsaBLERest: bad command %s\n", curCommand.c_str());
    return 400;
  }
  const char *url_c = url.c_str();
  IotsaBLERestApiService* service = nullptr;
  for(IotsaBLERestApiService* candidate : IotsaBLERestApiService::all) {
    if (cmd_get && !candidate->provider_get) continue;
    if (cmd_put && !candidate->provider_put) continue;
    if (cmd_post && !candidate->provider_post) continue;
    if (strcmp(candidate->provider_path, url_c) != 0) continue;
    service = candidate;
    break;
  }
  if (service == nullptr) {
    IotsaSerial.printf("IotsaBLERest: no api provider for url %s\n", url.c_str());
    return 404;
  }
  IotsaApiProvider* provider = service->provider;
  // xxxjson
  int jsonBufSize = 2048;
  ArduinoJson::JsonObject request;
  if (cmd_put || cmd_post) {
    ArduinoJson::DynamicJsonDocument request_doc(jsonBufSize);
    if (request_doc.overflowed()) {
      IotsaSerial.println("IotsaBLERest: request too large");
      return 413;
    }
    ArduinoJson::deserializeJson(request_doc, curData.c_str());
    request = request_doc.as<JsonObject>();
  }
  ArduinoJson::DynamicJsonDocument reply_doc(jsonBufSize);
  ArduinoJson::JsonObject reply = reply_doc.to<JsonObject>();
  bool ok = false;
  if (cmd_get) {
    ok = provider->getHandler(url_c, reply);
  } else
  if (cmd_put) {
    ok = provider->putHandler(url_c, request, reply);
  } else
  if (cmd_post) {
    ok = provider->postHandler(url_c, request, reply);
  }
  if (!ok) {
    IotsaSerial.printf("IotsaBLERest: bad request \"%s\"\n", url_c);
    return 400;
  }
  if (reply_doc.overflowed()) {
    IotsaSerial.println("IotsaBLERest: reply too large");
    return 413;
  }
  serializeJson(reply_doc, curResponse);
  IFBLEDEBUG IotsaSerial.printf("IotsaBLERest: reply(%d): %s\n", curResponse.size(), curResponse.c_str());
  curData = "";
  curCommand = "";
  return 200;
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
  IFBLEDEBUG IotsaSerial.printf("IotsaBLERestApiService: path=%s\n", path);
  provider_path = path;
  provider_get = get;
  provider_put = put;
  provider_post = post;
}