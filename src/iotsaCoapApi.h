#ifndef _IOTSACOAPAPI_H_
#define _IOTSACOAPAPI_H_
#include "iotsa.h"

class IotsaCoapServiceMod;

class IotsaCoapApiService : public IotsaApiServiceProvider {
public:
  IotsaCoapApiService(IotsaApiProvider* _provider, IotsaApplication &_app);
  IotsaCoapApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaWebServer* _server)
  : IotsaCoapApiService(_provider, _app)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false);
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
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
protected:
  IotsaCoapApiService api;
};


#endif
