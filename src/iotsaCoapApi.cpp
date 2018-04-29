#include "iotsaApi.h"
#ifdef IOTSA_WITH_COAP
#include <WiFiUdp.h>
#include <coap.h>

class CoapEndpoint {
public:
  CoapEndpoint(IotsaApiProvider *_provider, bool _get, bool _put, bool _post)
  : provider(_provider),
    get(_get),
    put(_put),
    post(_post),
    next(NULL)
  {}
  IotsaApiProvider *provider;
  bool get;
  bool put;
  bool post;
  CoapEndpoint *next;
  void callbackImpl(CoapPacket &pkt, IPAddress ip, int port);

//  CoapCallback myCallback();
};

void CoapEndpoint::callbackImpl(CoapPacket &pkt, IPAddress ip, int port) {

}

#if 0
CoapCallback CoapEndpoint::myCallback() {
    return &std::bind(&CoapEndpoint::callbackImpl, this);
}
#endif

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
    if (strncmp(path, "/api/", 5) == 0) path += 5;
    CoapEndpoint *ep = new CoapEndpoint(provider, get, put, post);
    _coapMod->addEndpoint(ep, path);
}

#endif