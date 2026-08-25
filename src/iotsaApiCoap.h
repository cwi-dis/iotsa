#ifndef _IOTSAAPICOAP_H_
#define _IOTSAAPICOAP_H_
#include "iotsa.h"

class IotsaCoapServiceMod;

class IotsaApiServiceCoap : public IotsaApiServiceProvider {
public:
  IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app);
  IotsaApiServiceCoap(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
  : IotsaApiServiceCoap(_provider, _app)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override;
  static void ensureServiceMod(IotsaApplication &app);
private:
  IotsaApiProvider* provider; 
//  void _getHandlerWrapper(const char *path);
//  void _putHandlerWrapper(const char *path);
//  void _postHandlerWrapper(const char *path);
 static IotsaCoapServiceMod *_coapMod;
};


class IotsaCoapApiMod : public IotsaBaseModule {
public:
  IotsaCoapApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBaseModule(_app, _auth, early),
    api(this, _app)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
protected:
  IotsaApiServiceCoap api;
};


#endif
