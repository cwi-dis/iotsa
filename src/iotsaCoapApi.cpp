#include "iotsaApi.h"
#ifdef IOTSA_WITH_COAP
#include <WiFiUdp.h>
#include <coap.h>

// Static variable
IotsaCoapServiceMod* IotsaCoapApiService::_coapMod = NULL;

class CoapEndpoint {
public:
  CoapEndpoint(IotsaApiProvider *_provider, const char *_path, bool _get, bool _put, bool _post)
  : provider(_provider),
    path(_path),
    get(_get),
    put(_put),
    post(_post),
    next(NULL),
    coap(NULL)
  {}
  IotsaApiProvider *provider;
  const char *path;
  bool get;
  bool put;
  bool post;
  CoapEndpoint *next;
  Coap* coap;

  void callbackImpl(CoapPacket &pkt, IPAddress ip, int port);

};

void CoapEndpoint::callbackImpl(CoapPacket &pkt, IPAddress ip, int port) {
    bool ok = false;
    String replyData;
    // Handle requests, after chaing that type (get/put/post) is supported.
    if (pkt.type == COAP_GET) {
        IFDEBUG IotsaSerial.print("COAP-GET api ");
        IFDEBUG IotsaSerial.println(path);
        ok = get;
        if (ok) {
            DynamicJsonBuffer replyBuffer;
            JsonObject& reply = replyBuffer.createObject();
            ok = provider->getHandler(path, reply);
            if (ok) {
                reply.printTo(replyData);
            }
        }
    }
    if (pkt.type == COAP_PUT) {
        IFDEBUG IotsaSerial.print("COAP-PUT api ");
        IFDEBUG IotsaSerial.println(path);
        ok = put && pkt.type == COAP_APPLICATION_JSON;
        if (ok) {
            char dataBuffer[pkt.payloadlen+1];
            memcpy(dataBuffer, pkt.payload, pkt.payloadlen);
            dataBuffer[pkt.payloadlen] = 0;
            DynamicJsonBuffer requestBuffer;
            JsonObject& request = requestBuffer.parseObject(dataBuffer);
            DynamicJsonBuffer replyBuffer;
            JsonObject& reply = replyBuffer.createObject();
            ok = provider->putHandler(path, request, reply);
            if (ok) {
                reply.printTo(replyData);
            }
        }
    }
    if (pkt.type == COAP_POST) {
        IFDEBUG IotsaSerial.print("COAP-POST api ");
        IFDEBUG IotsaSerial.println(path);
        ok = post && pkt.type == COAP_APPLICATION_JSON;
        if (ok) {
            char dataBuffer[pkt.payloadlen+1];
            memcpy(dataBuffer, pkt.payload, pkt.payloadlen);
            dataBuffer[pkt.payloadlen] = 0;
            DynamicJsonBuffer requestBuffer;
            JsonObject& request = requestBuffer.parseObject(dataBuffer);
            DynamicJsonBuffer replyBuffer;
            JsonObject& reply = replyBuffer.createObject();
            ok = provider->postHandler(path, request, reply);
            if (ok) {
                reply.printTo(replyData);
            }
        }
    }
    // Send reply, either a JSON datastructure or an error.
    if (ok) {
        coap->sendResponse(ip, port, pkt.messageid, replyData.c_str(), replyData.length(), COAP_CONTENT, COAP_APPLICATION_JSON, NULL, 0);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        coap->sendResponse(ip, port, pkt.messageid, NULL, 0, COAP_BAD_REQUEST, COAP_NONE, NULL, 0);
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}

class IotsaCoapServiceMod : public IotsaBaseMod {
public:
  IotsaCoapServiceMod(IotsaApplication &_app);
  void setup();
  void loop();
  void addEndpoint(CoapEndpoint *ep, const char *path);
protected:
  CoapEndpoint *firstEndpoint;
  WiFiUDP udp;
  Coap coap;
};

IotsaCoapServiceMod::IotsaCoapServiceMod(IotsaApplication &_app)
  : IotsaBaseMod(_app),
    firstEndpoint(NULL),
    udp(),
    coap(udp)
  {}

void IotsaCoapServiceMod::setup() {
    coap.start();
}

void IotsaCoapServiceMod::loop() {
    coap.loop();
}

void IotsaCoapServiceMod::addEndpoint(CoapEndpoint *ep, const char *path) {
    ep->next = firstEndpoint;
    firstEndpoint = ep;
    coap.server(std::bind(&CoapEndpoint::callbackImpl, ep, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), String(path));
}

IotsaCoapApiService::IotsaCoapApiService(IotsaApiProvider* _provider, IotsaApplication &_app)
  : provider(_provider)
{ 
  if (_coapMod == NULL) _coapMod = new IotsaCoapServiceMod(_app); 
}

void IotsaCoapApiService::setup(const char* path, bool get, bool put, bool post) {
    CoapEndpoint *ep = new CoapEndpoint(provider, path, get, put, post);
    if (strncmp(path, "/api/", 5) == 0) path += 5;
    _coapMod->addEndpoint(ep, path);
}

#endif