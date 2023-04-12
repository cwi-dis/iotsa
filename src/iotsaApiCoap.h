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
private:
  IotsaApiProvider* provider; 
//  void _getHandlerWrapper(const char *path);
//  void _putHandlerWrapper(const char *path);
//  void _postHandlerWrapper(const char *path);
 static IotsaCoapServiceMod *_coapMod;
};


class IotsaCoapApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaCoapApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early),
    api(this, _app)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
protected:
  IotsaApiServiceCoap api;
};


#endif
