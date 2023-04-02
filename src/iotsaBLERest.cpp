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

  IotsaSerial.println("IotsaBLERestMod: ble: write unknown uuid");
  return false;
}

bool IotsaBLERestMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == urlUUID) {
    std::string tmp = curUrl;
    bleApi.set(charUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: response url=%s\n", tmp.c_str());
    return true;
  }
  if (charUUID == bodyUUID) {
    std::string tmp = curBody;
    bleApi.set(charUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: response body=%s\n", tmp.c_str());
    return true;
  }
  if (charUUID == headersUUID) {
     std::string tmp = curHeaders;
    bleApi.set(charUUID, tmp);
    IFBLEDEBUG IotsaSerial.printf("IotsaBLERestMod: headers=%s\n", tmp.c_str());
    return true;
  } 
  if (charUUID == statusUUID) {
    uint8_t data[3];
    data[0] = curHttpStatus & 0xff;
    data[1] = (curHttpStatus >> 8) & 0xff;
    data[2] = (uint8_t)curDataStatus;
    bleApi.set(charUUID, data, 3);
    return true;
  }
  if (charUUID == securityUUID) {
    bleApi.set(charUUID, (uint8_t)0);
    return true;
  }
  
  IotsaSerial.println("IotsaBLERestMod: ble: read unknown uuid");
  return false;
}

int IotsaBLERestMod::_processRequest(HPSControl command) {
  IFDEBUG IotsaSerial.printf("BLEREST 0x%02x %s\n", (int)command, curUrl.c_str());
  bool cmd_get = command == HPSControl::GET;
  bool cmd_put = command == HPSControl::PUT;
  bool cmd_post = command == HPSControl::POST;
  if (!cmd_get && !cmd_put && !cmd_post) {
    IotsaSerial.printf("IotsaBLERest: bad command 0x%02x\n", command);
    return 400;
  }
  const char *url_c = curUrl.c_str();
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
    curDataStatus = HPSDataStatus::NONE;
  } else
  {
    curDataStatus = HPSDataStatus::BodyReceived;
  }
  // We don't do reply headers for now.
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