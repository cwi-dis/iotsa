#ifndef _IOTSAAPIWEB_H_
#define _IOTSAAPIWEB_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
//
// Registers a module's single web page, alongside (but independent of) the
// REST/CoAP/HPS transports registered via IotsaApiService/api.setup() -- see
// cwi-dis/iotsa#213. Deliberately not part of that chain-of-responsibility: a
// module has at most one page (unlike REST/CoAP/HPS, which register one
// endpoint per api.setup() call, including per-instance sub-paths), so a
// module simply calls web.setup() only for its own primary path and never
// for sub-paths like a button's own /buttons/N.
//
class IotsaWebServiceProvider {
public:
  IotsaWebServiceProvider(IotsaApiProvider* _provider, IotsaApplication &_app)
  : provider(_provider),
    server(_app.server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false);
private:
  IotsaApiProvider* provider;
  IotsaWebServer* server;
  void _webHandlerWrapper();
};
#endif // IOTSA_WITH_WEB
#endif
