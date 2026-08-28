#include "iotsaApi.h"
#ifdef IOTSA_HAS_COAPSERVER
#include <WiFiUdp.h>
#include <coap-simple.h>

#define COAP_PROTOCOL_DEBUG

// Static variable
IotsaCoapServiceMod* IotsaApiServiceCoap::_coapMod = NULL;

class CoapEndpoint {
public:
  CoapEndpoint(IotsaApiProvider *_provider, const String &_path, bool _get, bool _put, bool _post)
  : provider(_provider),
    path(_path),
    get(_get),
    put(_put),
    post(_post),
    coap(NULL)
  {}
  IotsaApiProvider *provider;
  // Owned copy: the module's own /api/-prefixed path, handed to getHandler/putHandler/
  // postHandler at request time, as opposed to the bare name used for our own CoAP
  // resource registration (which coap.server() below already copies into its own String).
  String path;
  bool get;
  bool put;
  bool post;
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
            ok = provider->getHandler(path.c_str(), reply);
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

            ok = provider->putHandler(path.c_str(), request, reply);
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
            ok = provider->postHandler(path.c_str(), request, reply);
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

class IotsaCoapServiceMod : public IotsaBaseModule {
public:
  IotsaCoapServiceMod(IotsaApplication &_app);
  void setup() override;
  void loop() override;
  void addEndpoint(CoapEndpoint *ep, const char *path);
protected:
  WiFiUDP udp;
  Coap coap;
};

IotsaCoapServiceMod::IotsaCoapServiceMod(IotsaApplication &_app)
  : IotsaBaseModule(_app),
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
    coap.server(ep->getCallback(&coap), String(path));
}

IotsaApiServiceCoap::IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaApiServiceProvider* _next)
  : IotsaApiServiceProvider(_next),
    provider(_provider)
{
  ensureServiceMod(_app);
}

void IotsaApiServiceCoap::ensureServiceMod(IotsaApplication &app) {
  if (_coapMod == NULL) _coapMod = new IotsaCoapServiceMod(app);
}

void IotsaApiServiceCoap::setup(const char* path, bool get, bool put, bool post, bool webPage) {
    if (iotsaConfig.wifiEnabled) {
        // CoAP has no use for an HTTP-ism in its own resource namespace, so it registers
        // the bare name directly; it still reconstructs /api/+name for what it hands to
        // the module's handlers, to keep that contract identical to REST/HPS.
        String fullPath = String("/api/") + path;
        CoapEndpoint *ep = new CoapEndpoint(provider, fullPath, get, put, post);
        _coapMod->addEndpoint(ep, path);
    }
    // webPage is Web-only; CoAP ignores it and just forwards it down the chain.
    if (next) next->setup(path, get, put, post, webPage);
}

#endif