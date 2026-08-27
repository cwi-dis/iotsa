#ifndef _IOTSAAPIWEB_H_
#define _IOTSAAPIWEB_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
//
// Registers a module's web page as a link in the same REST/CoAP/HPS transport
// chain as IotsaApiServiceRest/Coap/Hps -- see cwi-dis/iotsa#213. A single
// api.setup() call reaches every compiled-in transport, Web included; get=true
// simply means "this path also has a page" (put/post aren't used here, a page
// doesn't have separate variants the way REST/CoAP/HPS do).
//
// A module with a collection/sub-path pattern (buttons/N, users/N -- see
// cwi-dis/iotsa#217) ends up registering one redundant, identical page per
// sub-path this way; accepted for now, to be revisited under #217.
//
class IotsaApiServiceWeb : public IotsaApiServiceProvider {
public:
  IotsaApiServiceWeb(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next=nullptr)
  : IotsaApiServiceProvider(_next),
    provider(_provider),
    server(_app.server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override;
private:
  IotsaApiProvider* provider;
  IotsaWebServer* server;
  void _webHandlerWrapper();
};
#endif // IOTSA_WITH_WEB
#endif
