#include "iotsa.h"
#include "iotsaHps.h"
#include "iotsaHpsApi.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif

#ifdef IOTSA_WITH_WEB

String IotsaHpsMod::info() {
  String message = "<p>Built with support for REST over Bluetooth LE.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaHpsMod::getHandler(const char *path, JsonObject& reply) {
    return true;
}

bool IotsaHpsMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
    return false;
}

bool IotsaHpsMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == controlPointUUID) {
    HPSControl command = (HPSControl)bleApi.getAsInt(charUUID);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: request command=0x%02x\n", (int)command);
    curHttpStatus = _processRequest(command);
    return true;
  }
  if (charUUID == urlUUID) {
    curUrl = bleApi.getAsString(charUUID);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: request url=%s\n", curUrl.c_str());
    return true;
  }
  if (charUUID == bodyUUID) {
    curBody = bleApi.getAsString(charUUID);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: request body=%s\n", curBody.c_str());
    return true;
  }
  if (charUUID == headersUUID) {
    curHeaders = bleApi.getAsString(charUUID);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: request headers=%s\n", curHeaders.c_str());
    return true;
  } 

  IotsaSerial.printf("IotsaBLERestMod: ble: write unknown uuid %s\n", charUUID);
  return false;
}

bool IotsaHpsMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == urlUUID) {
    std::string tmp = curUrl;
    bleApi.set(urlUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: response url=%s\n", tmp.c_str());
    return true;
  }
  if (charUUID == bodyUUID) {
    std::string tmp = curBody;
    bleApi.set(bodyUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: response body=%s\n", tmp.c_str());
    return true;
  }
  if (charUUID == headersUUID) {
    std::string tmp = curHeaders;
    bleApi.set(headersUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: headers=%s\n", tmp.c_str());
    return true;
  } 
  if (charUUID == controlPointUUID) {
    bleApi.set(controlPointUUID, (uint8_t)HPSControl::NONE);
    return true;
  }
  if (charUUID == statusUUID) {
    uint8_t data[3];
    data[0] = curHttpStatus & 0xff;
    data[1] = (curHttpStatus >> 8) & 0xff;
    data[2] = (uint8_t)curDataStatus;
    bleApi.set(statusUUID, data, 3);
    return true;
  }
  if (charUUID == securityUUID) {
    bleApi.set(securityUUID, (uint8_t)0);
    return true;
  }
  
  IotsaSerial.printf("IotsaBLERestMod: ble: read unknown uuid %s\n", charUUID);
  return false;
}

int IotsaHpsMod::_processRequest(HPSControl command) {
  IFDEBUG IotsaSerial.printf("BLEREST 0x%02x %s\n", (int)command, curUrl.c_str());
  bool cmd_get = command == HPSControl::GET;
  bool cmd_put = command == HPSControl::PUT;
  bool cmd_post = command == HPSControl::POST;
  if (!cmd_get && !cmd_put && !cmd_post) {
    IotsaSerial.printf("IotsaBLERest: bad command 0x%02x\n", command);
    return 400;
  }
  const char *url_c = curUrl.c_str();
  IotsaHpsApiService* service = nullptr;
  for(IotsaHpsApiService* candidate : IotsaHpsApiService::all) {
    if (cmd_get && !candidate->provider_get) continue;
    if (cmd_put && !candidate->provider_put) continue;
    if (cmd_post && !candidate->provider_post) continue;
    if (strcmp(candidate->provider_path, url_c) != 0) continue;
    service = candidate;
    break;
  }
  if (service == nullptr) {
    IotsaSerial.printf("IotsaBLERest: no api provider for url %s\n", url_c);
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
    ArduinoJson::deserializeJson(request_doc, curBody.c_str());
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
  curBody = "";
  serializeJson(reply_doc, curBody);
  IFBLEDEBUG IotsaSerial.printf("IotsaBLERest: reply(%d): %s\n", curBody.size(), curBody.c_str());
  if (curBody.size() > HPSMaxBodySize) {
    curDataStatus = HPSDataStatus::BodyTruncated;
    curBody = curBody.substr(0, HPSMaxBodySize);
  } else
  if (curBody.size() == 0) {
    curDataStatus = HPSDataStatus::EMPTY;
  } else
  {
    curDataStatus = HPSDataStatus::BodyReceived;
  }
  // We don't do reply headers for now.
  // Set the statusUUID data. This will send a notification or indication if it was requested.
  uint8_t data[3];
  data[0] = curHttpStatus & 0xff;
  data[1] = (curHttpStatus >> 8) & 0xff;
  data[2] = (uint8_t)curDataStatus;
  bleApi.set(statusUUID, data, 3);
  return 200;
}

void IotsaHpsMod::setup() {
  bleApi.setup(serviceUUID, this);
  // Explain to clients what the rgb characteristic looks like
  bleApi.addCharacteristic(urlUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS URL");
  bleApi.addCharacteristic(headersUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS Headers");
  bleApi.addCharacteristic(statusUUID, BLE_READ|BLE_NOTIFY, BLE2904::FORMAT_OPAQUE, 0x2700, "HPS Status");
  bleApi.addCharacteristic(bodyUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS Body");
  bleApi.addCharacteristic(controlPointUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UINT8, 0x2700, "HPS ControlPoint");
  bleApi.addCharacteristic(securityUUID, BLE_READ, BLE2904::FORMAT_BOOLEAN, 0x2700, "HPS Security");
}

void IotsaHpsMod::serverSetup() {
  api.setup("/api/blehps", true, false);
  name = "blehps";
}

void IotsaHpsMod::loop() {
}

std::list<IotsaHpsApiService*> IotsaHpsApiService::all;

IotsaHpsApiService::IotsaHpsApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
: provider(_provider),
  auth(_auth)
{
  all.push_back(this);
}
  
void IotsaHpsApiService::setup(const char* path, bool get, bool put, bool post) {
  IFBLEDEBUG IotsaSerial.printf("IotsaBLERestApiService: path=%s\n", path);
  provider_path = path;
  provider_get = get;
  provider_put = put;
  provider_post = post;
}