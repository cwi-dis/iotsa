#include "iotsaApi.h"
#ifdef IOTSA_HAS_COAPSERVER
#include <WiFiUdp.h>
#include <coap-simple.h>

#define COAP_PROTOCOL_DEBUG

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
    IotsaActivityScope activity(IOTSA_CRUMB_REQUEST, IOTSA_CRUMB_COAP);
#ifdef COAP_PROTOCOL_DEBUG
    IotsaSerial.print("COAP pkt recvd from "); IotsaSerial.print(ip); IotsaSerial.print(" port "); IotsaSerial.println(port);
    IotsaSerial.print("type "); IotsaSerial.println(int(pkt.type));
    IotsaSerial.print("code "); IotsaSerial.println(int(pkt.code));
    IotsaSerial.print("tokenlen "); IotsaSerial.println(int(pkt.tokenlen));
    IotsaSerial.print("payloadlen "); IotsaSerial.println(int(pkt.payloadlen));
    IotsaSerial.print("messageid "); IotsaSerial.println(int(pkt.messageid));
    IotsaSerial.print("optionnum "); IotsaSerial.println(int(pkt.optionnum));
#endif
    // One request = one iotsaApiBegin()/iotsaApiEnd() pair, like REST and HPS (cwi-dis/iotsa#280).
    const char *method = pkt.code == COAP_GET ? "GET" : pkt.code == COAP_PUT ? "PUT" : pkt.code == COAP_POST ? "POST" : "?";
    iotsaApiBegin("coap", method, path.c_str());
    bool ok = false;
    JsonDocument replyDocument;
    JsonObject reply = replyDocument.to<JsonObject>();
    bool allowed = (pkt.code == COAP_GET && get) || (pkt.code == COAP_PUT && put) || (pkt.code == COAP_POST && post);
    if (!allowed) {
        iotsaApiError(405, "method not allowed");
    } else if (pkt.code == COAP_GET) {
        ok = provider->getHandler(path.c_str(), reply);
    } else {
        // xxxjack Should look through pkt.options looking for mimetype=application/json
        char dataBuffer[pkt.payloadlen+1];
        memcpy(dataBuffer, pkt.payload, pkt.payloadlen);
        dataBuffer[pkt.payloadlen] = 0;
        JsonDocument requestDocument;
        DeserializationError err = deserializeJson(requestDocument, dataBuffer);
        if (err == DeserializationError::NoMemory || requestDocument.overflowed()) {
            iotsaApiError(413, "request too big");
        } else if (err && err != DeserializationError::EmptyInput) {
            iotsaApiError(400, "invalid JSON");
        } else {
            JsonObject request = requestDocument.as<JsonObject>();
            ok = pkt.code == COAP_PUT ? provider->putHandler(path.c_str(), request, reply) : provider->postHandler(path.c_str(), request, reply);
        }
    }
    String body;
    int status = iotsaApiEnd(ok, replyDocument, body);
    // CoAP codes are class.detail, the same numbers as HTTP: 404 -> 4.04, 409 -> 4.09.
    COAP_RESPONSE_CODE code = status == 200 ? COAP_CONTENT : (COAP_RESPONSE_CODE)RESPONSE_CODE(status / 100, status % 100);
    if (!coap->sendResponse(ip, port, pkt.messageid, body.c_str(), body.length(), code, COAP_APPLICATION_JSON, pkt.token, pkt.tokenlen)) {
        coap->sendResponse(ip, port, pkt.messageid, NULL, 0, COAP_INTERNAL_SERVER_ERROR, COAP_NONE, pkt.token, pkt.tokenlen);
        IotsaSerial.println("-> COAP sendResponse error");
    }
#if 0
    // xxxjack no idea why this was added:
    delay(2000); // xxxjack
#endif
}

class IotsaCoapServiceMod : public IotsaBaseModule, public IotsaSingletonModule<IotsaCoapServiceMod> {
public:
  IotsaCoapServiceMod(IotsaApplication &_app);
  void setup() override;
  void loop() override;
  void addEndpoint(CoapEndpoint *ep, const char *path);
protected:
  WiFiUDP udp;
  Coap coap;
  bool _started = false;
  void _startIfReady();
};

IotsaCoapServiceMod::IotsaCoapServiceMod(IotsaApplication &_app)
  : IotsaBaseModule(_app),
    udp(),
    coap(udp)
  {
    claimSingleton(this);
  }

void IotsaCoapServiceMod::setup() {
    name = "coap";
    _startIfReady();
}

// Binds the UDP socket once the TCP/IP stack is up. Called every loop(), so
// WiFi coming up late is fine; same reasoning as IotsaHttpServiceMod's
// _startIfReady() (cwi-dis/iotsa#238, #239).
void IotsaCoapServiceMod::_startIfReady() {
    if (_started || !iotsaStatus.networkStackUp) return;
    coap.start();
    _started = true;
    IFDEBUG IotsaSerial.println("CoAP server started");
}

void IotsaCoapServiceMod::loop() {
    _startIfReady();
    if (!_started) return;
    coap.loop();
}

// Only adds to the CoAP library's URI table, no socket needed: always register,
// whatever the network state.
void IotsaCoapServiceMod::addEndpoint(CoapEndpoint *ep, const char *path) {
    coap.server(ep->getCallback(&coap), String(path));
}

IotsaApiServiceCoap::IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaApiServiceProvider* _next)
  : IotsaApiServiceProvider(_next),
    provider(_provider)
{
  ensureServiceMod(_app);
}

void IotsaApiServiceCoap::ensureServiceMod(IotsaApplication &app) {
  IotsaCoapServiceMod::ensure(app);
}

void IotsaApiServiceCoap::setup(const char* path, bool get, bool put, bool post, bool webPage) {
    // CoAP has no use for an HTTP-ism in its own resource namespace, so it registers
    // the bare name directly; it still reconstructs /api/+name for what it hands to
    // the module's handlers, to keep that contract identical to REST/HPS.
    String fullPath = String("/api/") + path;
    CoapEndpoint *ep = new CoapEndpoint(provider, fullPath, get, put, post);
    IotsaCoapServiceMod::instance()->addEndpoint(ep, path);
    // webPage is Web-only; CoAP ignores it and just forwards it down the chain.
    if (next) next->setup(path, get, put, post, webPage);
}

#endif