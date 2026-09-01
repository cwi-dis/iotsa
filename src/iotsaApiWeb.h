#ifndef _IOTSAAPIWEB_H_
#define _IOTSAAPIWEB_H_
#include "iotsa.h"
#include "iotsaHttpServer.h"

#ifdef IOTSA_WITH_WEB
//
// Registers a module's web page as a link in the same REST/CoAP/HPS transport
// chain as IotsaApiServiceRest/Coap/Hps -- see cwi-dis/iotsa#213. A single
// api.setup() call reaches every compiled-in transport, Web included; get=true
// simply means "this path also has a page" (put/post aren't used here, a page
// doesn't have separate variants the way REST/CoAP/HPS do), unless webPage=false
// overrides that -- see setup()'s doc comment on IotsaApiServiceProvider.
//
// A module with a collection/sub-path pattern (buttons/N, users/N) passes
// webPage=false for the per-item calls, since webHandler() doesn't distinguish
// by path and a page per item would just be a byte-identical duplicate of the
// collection's own page -- see cwi-dis/iotsa#217.
//
class IotsaApiServiceWeb : public IotsaApiServiceProvider {
public:
  IotsaApiServiceWeb(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next=nullptr)
  : IotsaApiServiceProvider(_next),
    // Init-list order mirrors declaration order (server before provider) to
    // avoid -Wreorder; neither initializer reads the other, so it's cosmetic.
    // server is shared with IotsaApiServiceRest, owned by neither -- see
    // cwi-dis/iotsa#207/#211. Guaranteed non-null: IotsaApplication's own
    // constructor ensures the shared mod exists before any module (this one
    // included) is constructed.
    server(IotsaHttpServiceMod::instance()->server),
    provider(_provider)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override;
  // Public so API-having modules can reach the shared HTTP server through their own
  // `api.webService` link (e.g. `api.webService->server`) instead of holding a
  // `server` field of their own -- see cwi-dis/iotsa#211.
  IotsaWebServer* server;
private:
  IotsaApiProvider* provider;
  void _webHandlerWrapper();
};
#endif // IOTSA_WITH_WEB
#endif
