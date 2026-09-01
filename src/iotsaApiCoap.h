#ifndef _IOTSAAPICOAP_H_
#define _IOTSAAPICOAP_H_
#include "iotsa.h"

class IotsaCoapServiceMod;

class IotsaApiServiceCoap : public IotsaApiServiceProvider {
public:
  IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaApiServiceProvider* _next=nullptr);
  IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next=nullptr)
  : IotsaApiServiceCoap(_provider, _app, _next)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override;
  static void ensureServiceMod(IotsaApplication &app);
private:
  IotsaApiProvider* provider;
//  void _getHandlerWrapper(const char *path);
//  void _putHandlerWrapper(const char *path);
//  void _postHandlerWrapper(const char *path);
};

#endif
