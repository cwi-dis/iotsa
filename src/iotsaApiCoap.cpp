#include "iotsaApi.h"
#ifdef IOTSA_WITH_COAP
#include <WiFiUdp.h>
#include <coap-simple.h>

#define COAP_PROTOCOL_DEBUG

// Static variable
IotsaCoapServiceMod* IotsaApiServiceCoap::_coapMod = NULL;

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
  CoapCallback getCallback(Coap *_coap);
  void callbackImpl(CoapPacket &pkt, IPAddress ip, int port);

};

CoapCallback CoapEndpoint::getCallback(Coap *_coap) {
    coap = _coap;
    return std::bind(&CoapEndpoint::callbackImpl, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

void CoapEndpoint::callbackImpl(CoapPacket &pkt, IPAddress ip, int port) {
#ifdef COAP_PROTOCOL_DEBUG
    IotsaSerial.print("COAP pkt recvd from "); IotsaSerial.print(ip); IotsaSerial.print(" port "); IotsaSerial.println(port);
    IotsaSerial.print("type "); IotsaSerial.println(int(pkt.type));
    IotsaSerial.print("code "); IotsaSerial.println(int(pkt.code));
    IotsaSerial.print("tokenlen "); IotsaSerial.println(int(pkt.tokenlen));
    IotsaSerial.print("payloadlen "); IotsaSerial.println(int(pkt.payloadlen));
    IotsaSerial.print("messageid "); IotsaSerial.println(int(pkt.messageid));
    IotsaSerial.print("optionnum "); IotsaSerial.println(int(pkt.optionnum));
#endif
    bool ok = false;
    String replyData;
    // Handle requests, after chaing that type (get/put/post) is supported.
    if (pkt.code == COAP_GET) {
        IFDEBUG IotsaSerial.print("COAP-GET api ");
        IFDEBUG IotsaSerial.println(path);
        ok = get;
        if (ok) {
            JsonDocument replyDocument;
            JsonObject reply = replyDocument.to<JsonObject>();
            ok = provider->getHandler(path, reply);
            if (ok) {
                serializeJson(replyDocument, replyData);
            }
        }
    } else
    if (pkt.code == COAP_PUT) {
        IFDEBUG IotsaSerial.print("COAP-PUT api ");
        IFDEBUG IotsaSerial.println(path);
        ok = put;
        // xxxjack Should look through pkt.options looking for mimetype=application/json
        if (ok) {
            char dataBuffer[pkt.payloadlen+1];
            memcpy(dataBuffer, pkt.payload, pkt.payloadlen);
            dataBuffer[pkt.payloadlen] = 0;
#ifdef COAP_PROTOCOL_DEBUG
            IotsaSerial.print("payload "); IotsaSerial.println(dataBuffer);
#endif
            JsonDocument requestDocument;
            deserializeJson(requestDocument, dataBuffer);
            JsonDocument replyDocument;
            JsonObject request = requestDocument.as<JsonObject>();
            JsonObject reply = replyDocument.to<JsonObject>();

            ok = provider->putHandler(path, request, reply);
            if (ok) {
                serializeJson(replyDocument, replyData);
            }
        }
    } else
    if (pkt.code == COAP_POST) {
        IFDEBUG IotsaSerial.print("COAP-POST api ");
        IFDEBUG IotsaSerial.println(path);
        ok = post;
        // xxxjack Should look through pkt.options looking for mimetype=application/json
        if (ok) {
            char dataBuffer[pkt.payloadlen+1];
            memcpy(dataBuffer, pkt.payload, pkt.payloadlen);
            dataBuffer[pkt.payloadlen] = 0;
#ifdef COAP_PROTOCOL_DEBUG
            IotsaSerial.print("payload "); IotsaSerial.println(dataBuffer);
#endif
            JsonDocument requestDocument;
            deserializeJson(requestDocument, dataBuffer);
            JsonDocument replyDocument;
            JsonObject request = requestDocument.as<JsonObject>();
            JsonObject reply = replyDocument.to<JsonObject>();
            ok = provider->postHandler(path, request, reply);
            if (ok) {
                serializeJson(replyDocument, replyData);
            }
        }
    } else {
        IFDEBUG IotsaSerial.print("COAP-UNKNOWN ");
        IFDEBUG IotsaSerial.println(int(pkt.code));
    }
    // Send reply, either a JSON datastructure or an error.
    if (ok) {
#ifdef COAP_PROTOCOL_DEBUG
        IotsaSerial.print("replyData "); IotsaSerial.println(replyData);
        IotsaSerial.print("replyLen "); IotsaSerial.println(replyData.length());
#endif
        int messageid = coap->sendResponse(ip, port, pkt.messageid, replyData.c_str(), replyData.length(), COAP_CONTENT, COAP_APPLICATION_JSON, pkt.token, pkt.tokenlen);
        if (messageid) {
            IFDEBUG IotsaSerial.println("-> OK");
        } else {
            coap->sendResponse(ip, port, pkt.messageid, NULL, 0, COAP_INTERNAL_SERVER_ERROR, COAP_NONE, pkt.token, pkt.tokenlen);
            IotsaSerial.println("-> COAP sendResponse error");
        }
    } else {
        coap->sendResponse(ip, port, pkt.messageid, NULL, 0, COAP_BAD_REQUEST, COAP_NONE, pkt.token, pkt.tokenlen);
        IFDEBUG IotsaSerial.println("-> ERR");
    }
#if 0
    // xxxjack no idea why this was added:
    delay(2000); // xxxjack
#endif
}

class IotsaCoapServiceMod : public IotsaBaseMod {
public:
  IotsaCoapServiceMod(IotsaApplication &_app);
  void setup() override;
  void loop() override;
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
    name = "coap";
    if (!iotsaConfig.wifiEnabled) return;
    coap.start();
}

void IotsaCoapServiceMod::loop() {
    if (!iotsaConfig.wifiEnabled) return;
    coap.loop();
}

void IotsaCoapServiceMod::addEndpoint(CoapEndpoint *ep, const char *path) {
    if (!iotsaConfig.wifiEnabled) return;
    ep->next = firstEndpoint;
    firstEndpoint = ep;
    coap.server(ep->getCallback(&coap), String(path));
}

IotsaApiServiceCoap::IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app)
  : provider(_provider)
{ 
  if (_coapMod == NULL) _coapMod = new IotsaCoapServiceMod(_app); 
}

void IotsaApiServiceCoap::setup(const char* path, bool get, bool put, bool post) {
    if (!iotsaConfig.wifiEnabled) return;
    CoapEndpoint *ep = new CoapEndpoint(provider, path, get, put, post);
    if (strncmp(path, "/api/", 5) == 0) path += 5;
    _coapMod->addEndpoint(ep, path);
}

#endif