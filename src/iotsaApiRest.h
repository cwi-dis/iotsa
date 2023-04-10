#ifndef _IOTSAAPIREST_H_
#define _IOTSAAPIREST_H_
#include "iotsa.h"

class IotsaApiServiceRest : public IotsaApiServiceProvider {
public:
  IotsaApiServiceRest(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
  : provider(_provider),
    auth(_auth),
    server(_app.server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override;
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
    api(this, _app, _auth)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
protected:
  IotsaApiServiceRest api;
};
#endif
