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
   // Owned copy: the caller now passes a bare name, so this class builds and keeps its
   // own /api/-prefixed path rather than aliasing the caller's (possibly transient) pointer.
   String api_path;
   IotsaApiProvider *provider;
   IotsaAuthenticationProvider* auth;
};

class IotsaHpsServiceMod : public IotsaBaseModule {
  friend class IotsaApiServiceHps;

  const int HPSMaxBodySize = 512;
  // Chunking-opt-in flag (#139), ORed into the controlPoint command byte. Command codes
  // are a small enum (<=4 today, per the real HPS spec), so the top bit is safely free.
  // A client that doesn't set it (any pre-#139 iotsa build, or a genuine third-party HPS
  // client) gets exactly the old single-write/single-read behavior below - the extension
  // is invisible unless a client explicitly asks for it.
  const uint8_t HPSControlChunkingFlag = 0x80;
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
  IotsaHpsServiceMod(IotsaApplication &_app) : IotsaBaseModule(_app) {}
  void setup() override {
    IotsaApiServiceHps::_hpsMod = this;
    name = "hps";
  }

  void loop() override {}
  // BLE characteristic registration used to happen in setup() rather than here, out
  // of step with REST/CoAP/Web -- moved once the reason for that split (a WiFi gate
  // that no longer exists, and BLE advertising ordering, now handled by
  // lateSetupDone()) turned out to no longer apply, see cwi-dis/iotsa#210.
  void lateSetup() override {
    IFBLEDEBUG IotsaSerial.println("IotsaHpsServiceMod::lateSetup called");
    bleApi.setup(IotsaApiServiceHps::serviceUUID, this);
    // Explain to clients what the rgb characteristic looks like
    bleApi.addCharacteristic(IotsaApiServiceHps::urlUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UTF8, 0x2700, "HPS URL");
    bleApi.addCharacteristic(IotsaApiServiceHps::headersUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UTF8, 0x2700, "HPS Headers");
    bleApi.addCharacteristic(IotsaApiServiceHps::statusUUID, bleApi.BLE_READ|bleApi.BLE_NOTIFY, NimBLE2904::FORMAT_OPAQUE, 0x2700, "HPS Status");
    bleApi.addCharacteristic(IotsaApiServiceHps::bodyUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UTF8, 0x2700, "HPS Body");
    bleApi.addCharacteristic(IotsaApiServiceHps::controlPointUUID, bleApi.BLE_READ|bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "HPS ControlPoint");
    bleApi.addCharacteristic(IotsaApiServiceHps::securityUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_BOOLEAN, 0x2700, "HPS Security");
  }
#ifdef IOTSA_WITH_WEB
  String info() override { return ""; }
#endif
protected:
  IotsaBleApiService bleApi;

  std::string curUrl;
  std::string curHeaders;
  std::string curBody;
  // Legacy (non-chunking) request body: only ever the single most recent bodyUUID write,
  // never accumulated - matches plain GATT replace-on-write semantics, so a non-chunking
  // client that (for whatever ordinary reason) writes bodyUUID more than once before
  // controlPoint gets "last write wins" instead of curBody's silently spliced result.
  std::string lastBodyChunk;
  // Whether the in-progress/most recent reply was generated for a chunking-enabled
  // request; controls whether bleGetHandler's bodyUUID case serves a cursor-advancing
  // chunk or the whole (legacy, <=HPSMaxBodySize truncated) body on every read.
  bool curReplyIsChunked = false;
  size_t curBodyReadOffset = 0;
  HPSControl curControl;
  uint16_t curHttpStatus;
  HPSDataStatus curDataStatus;

  static std::list<IotsaHpsServiceEntryPoint*> getEntryPoints;
  static std::list<IotsaHpsServiceEntryPoint*> putEntryPoints;
  static std::list<IotsaHpsServiceEntryPoint*> postEntryPoints;

  bool blePutHandler(UUIDstring charUUID) override {
    if (charUUID == IotsaApiServiceHps::controlPointUUID) {
      uint8_t raw = (uint8_t)bleApi.getAsInt(charUUID);
      bool chunking = (raw & HPSControlChunkingFlag) != 0;
      HPSControl command = (HPSControl)(raw & (uint8_t)~HPSControlChunkingFlag);
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request command=0x%02x chunking=%d\n", (int)command, (int)chunking);
      curHttpStatus = _processRequest(command, chunking);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::urlUUID) {
      curUrl = bleApi.getAsString(charUUID);
      // A url write starts a new request: reset the accumulated body from any
      // previous request (see bodyUUID handling below for the chunked-write scheme).
      curBody = "";
      lastBodyChunk = "";
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: request url=%s\n", curUrl.c_str());
      return true;
    }
    if (charUUID == IotsaApiServiceHps::bodyUUID) {
      // Chunked writes (#139): a request body larger than one GATT write (BLE's ATT
      // attribute-value hard cap is 512 bytes) is sent as several writes to this same
      // characteristic. Append rather than replace; curUrl (above) clears curBody at
      // the start of each new request, and controlPointUUID (below) finalizes it.
      // We accumulate unconditionally here because the chunking flag only arrives
      // later, on the controlPoint write; lastBodyChunk keeps the plain single-write
      // interpretation available too, for when it turns out not to be a chunking
      // request (see _processRequest).
      std::string chunk = bleApi.getAsString(charUUID);
      lastBodyChunk = chunk;
      curBody += chunk;
      IotsaSerial.printf("IotsaHpsServiceMod: request body chunk (%d bytes, total %d)\n", (int)chunk.size(), (int)curBody.size());
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
    IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: bleGetHandler(%s)\n", charUUID);
    if (charUUID == IotsaApiServiceHps::urlUUID) {
      std::string tmp = curUrl;
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: response url=%s\n", tmp.c_str());
      bleApi.set(IotsaApiServiceHps::urlUUID, tmp);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::bodyUUID) {
      std::string tmp;
      if (curReplyIsChunked) {
        // Chunked replies (#139): serve curBody in HPSMaxBodySize-sized slices,
        // advancing a read cursor each call, until an empty read signals the end -
        // the read-side mirror of the chunked-write scheme above. Only enabled when
        // the triggering request asked for it (curReplyIsChunked), so a client that
        // didn't ask for chunking always gets the old single-read, non-cursor
        // behavior below, which stays safely idempotent under e.g. transport retries.
        size_t remaining = curBody.size() > curBodyReadOffset ? curBody.size() - curBodyReadOffset : 0;
        size_t chunkSize = remaining < (size_t)HPSMaxBodySize ? remaining : (size_t)HPSMaxBodySize;
        tmp = curBody.substr(curBodyReadOffset, chunkSize);
        curBodyReadOffset += chunkSize;
      } else {
        tmp = curBody;
      }
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: response body=%s\n", tmp.c_str());
      bleApi.set(IotsaApiServiceHps::bodyUUID, tmp);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::headersUUID) {
      std::string tmp = curHeaders;
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: headers=%s\n", tmp.c_str());
      bleApi.set(IotsaApiServiceHps::headersUUID, tmp);
      return true;
    } 
    if (charUUID == IotsaApiServiceHps::controlPointUUID) {
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: control point\n");
      bleApi.set(IotsaApiServiceHps::controlPointUUID, (uint8_t)HPSControl::NONE);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::statusUUID) {
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: status\n");
      uint8_t data[3];
      data[0] = curHttpStatus & 0xff;
      data[1] = (curHttpStatus >> 8) & 0xff;
      data[2] = (uint8_t)curDataStatus;
      bleApi.set(IotsaApiServiceHps::statusUUID, data, 3);
      return true;
    }
    if (charUUID == IotsaApiServiceHps::securityUUID) {
      IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: security\n");
      bleApi.set(IotsaApiServiceHps::securityUUID, (uint8_t)0);
      return true;
    }
    
    IotsaSerial.printf("IotsaHpsServiceMod: ble: read unknown uuid %s\n", charUUID);
    return false;
  }

  int _processRequest(HPSControl command, bool chunking) {
    IFDEBUG IotsaSerial.printf("HPS 0x%02x %s chunking=%d\n", (int)command, curUrl.c_str(), (int)chunking);
    // Every reply generated from here on (including the early-return error paths
    // below) is read back under this request's chunking mode.
    curReplyIsChunked = chunking;
    curBodyReadOffset = 0;
    // The request body itself: the full chunked accumulation if the client asked for
    // chunking, otherwise just the single most recent bodyUUID write (see
    // lastBodyChunk's declaration for why that's the correct legacy interpretation).
    std::string requestBody = chunking ? curBody : lastBodyChunk;
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
      if (ep->api_path == url_c) {
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
    ArduinoJson::deserializeJson(request_doc, requestBody.c_str());
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
    // A chunking-enabled client reads the reply back over several chunked reads (see
    // bleGetHandler's bodyUUID case), so there's nothing to truncate here; a
    // non-chunking client only ever does a single read, so it's still bound to the
    // old single-read (ATT hard cap) limit.
    if (!chunking && curBody.size() > (size_t)HPSMaxBodySize) {
      curDataStatus = (HPSDataStatus)(HPSDataStatus::BodyTruncated | HPSDataStatus::BodyReceived);
      curBody = curBody.substr(0, HPSMaxBodySize);
    } else
    if (curBody.size() == 0) {
      curDataStatus = HPSDataStatus::EMPTY;
    } else
    {
      curDataStatus = HPSDataStatus::BodyReceived;
    }
    IFBLEDEBUG IotsaSerial.printf("IotsaHpsServiceMod: reply(%d, status=0x%x): %s\n", curBody.size(), (int)curDataStatus, curBody.c_str());
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

IotsaApiServiceHps::IotsaApiServiceHps(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next)
: IotsaApiServiceProvider(_next),
  auth(_auth)
{
  ensureServiceMod(_app);
  provider = _provider;
}

void IotsaApiServiceHps::ensureServiceMod(IotsaApplication &app) {
  if (_hpsMod == NULL) {
    IotsaSerial.println("IotsaApiServiceHps: allocate IotsaHpsServiceMod");
    _hpsMod = new IotsaHpsServiceMod(app);
  }
}
  
void IotsaApiServiceHps::setup(const char* path, bool get, bool put, bool post) {
  IFBLEDEBUG IotsaSerial.printf("IotsaApiServiceHps: path=%s\n", path);
  // The IotsaHpsServiceEntryPoint are immutable so we can add it to multiple lists
  IotsaHpsServiceEntryPoint *entry = new IotsaHpsServiceEntryPoint();
  entry->api_path = String("/api/") + path;
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
  if (next) next->setup(path, get, put, post);
}

#endif // IOTSA_WITH_HPS