#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiModObject : public IotsaModObject {
public:
  virtual ~IotsaApiModObject() {}
#ifdef IOTSA_WITH_API
  virtual void getHandler(JsonObject& reply) = 0;
  virtual bool putHandler(const JsonVariant& request) = 0;
#endif
};

class IotsaApiProvider  {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  virtual bool getHandler(const char *path, JsonObject& reply) = 0;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
};

class IotsaApiServiceProvider {
public:
  IotsaApiServiceProvider() {}
  virtual ~IotsaApiServiceProvider() {}
  virtual void setup(const char* path, bool get=false, bool put=false, bool post=false) = 0;
};

#ifdef IOTSA_WITH_REST
#include "iotsaRestApi.h"
#endif
#ifdef IOTSA_WITH_COAP
#include "iotsaCoapApi.h"
#endif

#if defined(IOTSA_WITH_REST) && defined(IOTSA_WITH_COAP)
//
// Class that provides both REST and COAP service endpoint implementations.
//
class IotsaANYApiService : public IotsaApiServiceProvider {
public:
  IotsaANYApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
  : restService(_provider, _app, _auth),
    coapService(_provider, _app, _auth)
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
    api(this, _app, _auth)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
protected:
  IotsaANYApiService api;
};
typedef IotsaANYApiMod IotsaApiMod;
typedef IotsaANYApiService IotsaApiService;
#elif defined(IOTSA_WITH_COAP)
typedef IotsaCoapApiMod IotsaApiMod;
typedef IotsaCoapApiService IotsaApiService;
#elif defined(IOTSA_WITH_REST)
typedef IotsaRestApiMod IotsaApiMod;
typedef IotsaRestApiService IotsaApiService;
#elif IOTSA_WITH_PLACEHOLDERS
// Don't define IotsaApiMod
class IotsaNoApiService : public IotsaApiServiceProvider {
public:
  IotsaNoApiService() {}
  IotsaNoApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth) {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) {}
};

class IotsaNoApiMod : public IotsaMod {
public:
  IotsaNoApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early)
  {}
protected:
  IotsaNoApiService api;
};

typedef IotsaNoApiMod IotsaApiMod;
typedef IotsaNoApiService IotsaApiService;
#else
// Don't define IotsaApiMod
#endif // IOTSA_WITH_REST, IOTSA_WITH_COAP
#endif // _IOTSAAPI_H_
