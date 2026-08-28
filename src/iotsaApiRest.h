#ifndef _IOTSAAPIREST_H_
#define _IOTSAAPIREST_H_
#include "iotsa.h"
#include "iotsaHttpServer.h"

class IotsaApiServiceRest : public IotsaApiServiceProvider {
public:
  IotsaApiServiceRest(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next=nullptr)
  : IotsaApiServiceProvider(_next),
    provider(_provider),
    auth(_auth),
    // Shared with IotsaApiServiceWeb, owned by neither -- see cwi-dis/iotsa#207/#211.
    // Guaranteed non-null: IotsaApplication's own constructor ensures the shared mod
    // exists before any module (this one included) is constructed.
    server(IotsaHttpServiceMod::serviceMod(_app)->server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override;
private:
  IotsaApiProvider* provider; 
  IotsaAuthenticationProvider* auth;
  IotsaWebServer* server;
  void _getHandlerWrapper(const char *path);
  void _putHandlerWrapper(const char *path);
  void _postHandlerWrapper(const char *path);
};
#endif
