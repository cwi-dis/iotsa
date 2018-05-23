#ifndef _IOTSARESTAPI_H_
#define _IOTSARESTAPI_H_
#include "iotsa.h"

class IotsaRestApiService : public IotsaApiServiceProvider {
public:
  IotsaRestApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
  : IotsaRestApiService(_provider, _auth, app.server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false);
private:
  IotsaApiProvider* provider; 
  IotsaAuthenticationProvider* auth;
  IotsaWebServer* server;
  void _getHandlerWrapper(const char *path);
  void _putHandlerWrapper(const char *path);
  void _postHandlerWrapper(const char *path);
};

class IotsaRestApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaRestApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early),
    api(this, _auth, server)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
protected:
  IotsaRestApiService api;
};
#endif
