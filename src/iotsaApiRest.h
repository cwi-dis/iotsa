#ifndef _IOTSAAPIREST_H_
#define _IOTSAAPIREST_H_
#include "iotsa.h"
#include "iotsaHttpServer.h"

class IotsaApiServiceRest : public IotsaApiServiceProvider {
public:
  IotsaApiServiceRest(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaApiServiceProvider* _next=nullptr)
  : IotsaApiServiceProvider(_next),
    provider(_provider),
    app(_app),
    // Shared with IotsaApiServiceWeb, owned by neither -- see cwi-dis/iotsa#207/#211.
    // Guaranteed non-null: IotsaApplication's own constructor ensures the shared mod
    // exists before any module (this one included) is constructed.
    server(IotsaHttpServiceMod::instance()->server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override;
private:
  IotsaApiProvider* provider; 
  IotsaApplication& app;
  IotsaWebServer* server;
  void _handle(IotsaApiOperation verb, const char *path);
};
#endif
