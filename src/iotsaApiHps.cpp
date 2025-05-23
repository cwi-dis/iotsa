#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

#ifdef IOTSA_WITH_HPS
#include "iotsaBLEServer.h"

#ifdef IOTSA_BLE_DEBUG
#define IFBLEDEBUG if(1)
#else
#define IFBLEDEBUG if(0)
#endif


class IotsaHpsServiceEntryPoint {
  public:
   const char *api_path;
   IotsaApiProvider *provider;
   IotsaAuthenticationProvider* auth;
};

class IotsaHpsServiceMod : public IotsaBaseMod, public IotsaBLEApiProvider {
  friend class IotsaApiServiceHps;

  const int HPSMaxBodySize = 512;
  enum HPSControl {
    NONE=0,
    GET=0x01,
    POST=0x03,
    PUT=0x04
  };
  enum HPSDataStatus {
    EMPTY=0,
    HeadersReceived = 0x01,
    HeadersTruncated = 0x02,
    BodyReceived = 0x04,
    BodyTruncated = 0x08
  };
public:
  using IotsaBaseMod::IotsaBaseMod;
  void setup() override {
    IotsaApiServiceHps::_hpsMod = this;
    name = "hps";
    IFBLEDEBUG IotsaSerial.println("IotsaHpsServiceMod::setup called");
    bleApi.setup(IotsaApiServiceHps::serviceUUID, this);
    // Explain to clients what the rgb characteristic looks like
    bleApi.addCharacteristic(IotsaApiServiceHps::urlUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS URL");
    bleApi.addCharacteristic(IotsaApiServiceHps::headersUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS Headers");
    bleApi.addCharacteristic(IotsaApiServiceHps::statusUUID, BLE_READ|BLE_NOTIFY, BLE2904::FORMAT_OPAQUE, 0x2700, "HPS Status");
    bleApi.addCharacteristic(IotsaApiServiceHps::bodyUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UTF8, 0x2700, "HPS Body");
    bleApi.addCharacteristic(IotsaApiServiceHps::controlPointUUID, BLE_READ|BLE_WRITE, BLE2904::FORMAT_UINT8, 0x2700, "HPS ControlPoint");
    bleApi.addCharacteristic(IotsaApiServiceHps::securityUUID, BLE_READ, BLE2904::FORMAT_BOOLEAN, 0x2700, "HPS Security");
  }

  void loop() override {}
protected:
  IotsaBleApiService bleApi;

  std::string curUrl;
  std::string curHeaders;
  std::string curBody;
  HPSControl curControl;
  uint16_t curHttpStatus;
  HPSDataStatus curDataStatus;

  static std::list<IotsaHpsServiceEntryPoint*> getEntryPoints;
  static std::list<IotsaHpsServiceEntryPoint*> putEntryPoints;
  static std::list<IotsaHpsServiceEntryPoint*> postEntryPoints;

  bool blePutHandler(UUIDstring charUUID) override {
    if (charUUID == IotsaApiServiceHps::controlPointUUID) {
      HPSControl command = (HPSControl)bleApi.getAsInt(charUUID);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request command=0x%02x\n", (int)command);
      curHttpStatus = _processRequest(command);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::urlUUID) {
      curUrl = bleApi.getAsString(charUUID);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request url=%s\n", curUrl.c_str());
      return true;
    }
    if (charUUID == IotsaApiServiceHps::bodyUUID) {
      curBody = bleApi.getAsString(charUUID);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request body=%s\n", curBody.c_str());
      return true;
    }
    if (charUUID == IotsaApiServiceHps::headersUUID) {
      curHeaders = bleApi.getAsString(charUUID);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request headers=%s\n", curHeaders.c_str());
      return true;
    } 

    IotsaSerial.printf("IotsaHpsServiceMod: ble: write unknown uuid %s\n", charUUID);
    return false;
  }

  bool bleGetHandler(UUIDstring charUUID) override {
    if (charUUID == IotsaApiServiceHps::urlUUID) {
      std::string tmp = curUrl;
      bleApi.set(IotsaApiServiceHps::urlUUID, tmp);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: response url=%s\n", tmp.c_str());
      return true;
    }
    if (charUUID == IotsaApiServiceHps::bodyUUID) {
      std::string tmp = curBody;
      bleApi.set(IotsaApiServiceHps::bodyUUID, tmp);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: response body=%s\n", tmp.c_str());
      return true;
    }
    if (charUUID == IotsaApiServiceHps::headersUUID) {
      std::string tmp = curHeaders;
      bleApi.set(IotsaApiServiceHps::headersUUID, tmp);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: headers=%s\n", tmp.c_str());
      return true;
    } 
    if (charUUID == IotsaApiServiceHps::controlPointUUID) {
      bleApi.set(IotsaApiServiceHps::controlPointUUID, (uint8_t)HPSControl::NONE);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::statusUUID) {
      uint8_t data[3];
      data[0] = curHttpStatus & 0xff;
      data[1] = (curHttpStatus >> 8) & 0xff;
      data[2] = (uint8_t)curDataStatus;
      bleApi.set(IotsaApiServiceHps::statusUUID, data, 3);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::securityUUID) {
      bleApi.set(IotsaApiServiceHps::securityUUID, (uint8_t)0);
      return true;
    }
    
    IotsaSerial.printf("IotsaHpsServiceMod: ble: read unknown uuid %s\n", charUUID);
    return false;
  }

  int _processRequest(HPSControl command) {
    IFDEBUG IotsaSerial.printf("HPS 0x%02x %s\n", (int)command, curUrl.c_str());
    bool cmd_get = command == HPSControl::GET;
    bool cmd_put = command == HPSControl::PUT;
    bool cmd_post = command == HPSControl::POST;
    if (!cmd_get && !cmd_put && !cmd_post) {
      IotsaSerial.printf("IotsaHpsServiceMod: bad command 0x%02x\n", command);
      curDataStatus = HPSDataStatus::EMPTY;
      curBody = "";
      return 400;
    }
    const char *url_c = curUrl.c_str();
    IotsaApiProvider* provider = nullptr;
    std::list<IotsaHpsServiceEntryPoint *>&epList =
      (cmd_get ? getEntryPoints :
      cmd_put ? putEntryPoints :
                postEntryPoints);
    for(IotsaHpsServiceEntryPoint* ep : epList) {
      if (strcmp(url_c, ep->api_path) == 0) {
        provider = ep->provider;
      }
    }
    if (provider == nullptr) {
      IotsaSerial.printf("IotsaHpsServiceMod: no api provider for command 0x%x url %s\n", command, url_c);
      curDataStatus = HPSDataStatus::EMPTY;
      curBody = "";
      return 404;
    }
    // xxxjson
    ArduinoJson::JsonDocument request_doc;
    ArduinoJson::deserializeJson(request_doc, curBody.c_str());
    if (request_doc.overflowed()) {
      IotsaSerial.println("IotsaHpsServiceMod: request too large");
      curDataStatus = HPSDataStatus::EMPTY;
      curBody = "";
      return 413;
    }
    ArduinoJson::JsonObject request = request_doc.as<JsonObject>();
    ArduinoJson::JsonDocument reply_doc;
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
      IotsaSerial.printf("IotsaHpsServiceMod: bad request \"%s\"\n", url_c);
      curDataStatus = HPSDataStatus::EMPTY;
      curBody = "";
      return 400;
    }
    if (reply_doc.overflowed()) {
      IotsaSerial.println("IotsaHpsServiceMod: reply too large");
      curDataStatus = HPSDataStatus::EMPTY;
      curBody = "";
      return 413;
    }
    curBody = "";
    serializeJson(reply_doc, curBody);
    IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: reply(%d): %s\n", curBody.size(), curBody.c_str());
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
    bleApi.set(IotsaApiServiceHps::statusUUID, data, 3);
    return 200;
  }
};

IotsaHpsServiceMod* IotsaApiServiceHps::_hpsMod = NULL;
std::list<IotsaHpsServiceEntryPoint*> IotsaHpsServiceMod::getEntryPoints;
std::list<IotsaHpsServiceEntryPoint*> IotsaHpsServiceMod::putEntryPoints;
std::list<IotsaHpsServiceEntryPoint*> IotsaHpsServiceMod::postEntryPoints;

IotsaApiServiceHps::IotsaApiServiceHps(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
: auth(_auth)
{
  if (_hpsMod == NULL) {
    IotsaSerial.println("IotsaApiServiceHps: allocate IotsaHpsServiceMod");
    _hpsMod = new IotsaHpsServiceMod(_app); 
  }
  provider = _provider;
}
  
void IotsaApiServiceHps::setup(const char* path, bool get, bool put, bool post) {
  IFBLEDEBUG IotsaSerial.printf("IotsaApiServiceHps: path=%s\n", path);
  // The IotsaHpsServiceEntryPoint are immutable so we can add it to multiple lists
  IotsaHpsServiceEntryPoint *entry = new IotsaHpsServiceEntryPoint();
  entry->api_path = path;
  entry->provider = provider;
  entry->auth = auth;
  if (get) {
    IotsaHpsServiceMod::getEntryPoints.push_back(entry);
  }
  if (put) {
    IotsaHpsServiceMod::putEntryPoints.push_back(entry);
  }
  if (post) {
    IotsaHpsServiceMod::postEntryPoints.push_back(entry);
  }
}

#endif // IOTSA_WITH_HPS