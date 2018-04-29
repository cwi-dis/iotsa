#ifndef _IOTSACOAPAPI_H_
#define _IOTSACOAPAPI_H_
#include "iotsa.h"
#include "IotsaApi.h"

class IotsaCoapServiceMod : public IotsaBaseMod {
public:
  IotsaCoapServiceMod(IotsaApplication &_app)
  : IotsaBaseMod(_app)
  {}
  void setup();
  void loop();
};

class IotsaCoapApiService : public IotsaApiService {
public:
  IotsaCoapApiService(IotsaApiProvider* _provider, IotsaApplication &_app)
  : provider(_provider)
  { 
    if (_coapMod == NULL) _coapMod = new IotsaCoapServiceMod(_app); 
  }
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

//
// Class that provides both REST and COAP service endpoint implementations.
//
class IotsaANYApiService : public IotsaApiService {
public:
  IotsaANYApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaWebServer& _server)
  : restService(_provider, _auth, _server),
    coapService(_provider, _app)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) {
    restService.setup(path, get, put, post);
    coapService.setup(path, get, put, post);
  }
private:
  IotsaRestApiService restService;
  IotsaCoapApiService coapService;
};

class IotsaANYApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaANYApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early),
    api(this, _app, _auth, server)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
protected:
  IotsaANYApiService api;
};

#endif
