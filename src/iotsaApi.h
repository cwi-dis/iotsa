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
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    if (reqObj[name].is<JT>()) {
      var = reqObj[name].as<CT>();
      return true;
    }
    return false;
  }
#endif
};

class IotsaApiProvider  {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  virtual bool getHandler(const char *path, JsonObject& reply) = 0;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    if (reqObj[name].is<JT>()) {
      var = reqObj[name].as<CT>();
      return true;
    }
    return false;
  }
};

class IotsaApiServiceProvider {
public:
  IotsaApiServiceProvider() {}
  virtual ~IotsaApiServiceProvider() {}
  virtual void setup(const char* path, bool get=false, bool put=false, bool post=false) = 0;
};

#ifdef IOTSA_WITH_REST
#include "iotsaApiRest.h"
#endif
#ifdef IOTSA_WITH_COAP
#include "iotsaApiCoap.h"
#endif
#ifdef IOTSA_WITH_HPS
#include "iotsaApiHps.h"
#endif

//
// Class that provides both REST and COAP service endpoint implementations.
//
class IotsaApiService : public IotsaApiServiceProvider {
public:
  IotsaApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth)
  :
  #ifdef IOTSA_WITH_REST
    restService(_provider, _app, _auth),
  #endif
  #ifdef IOTSA_WITH_COAP
    coapService(_provider, _app, _auth),
  #endif
  #ifdef IOTSA_WITH_HPS
    bleRestService(_provider, _app, _auth),
  #endif
    _dummy(0)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override {
  #ifdef IOTSA_WITH_REST
    restService.setup(path, get, put, post);
  #endif
  #ifdef IOTSA_WITH_COAP
    coapService.setup(path, get, put, post);
  #endif
  #ifdef IOTSA_WITH_HPS
    bleRestService.setup(path, get, put, post);
  #endif
  }
private:
#ifdef IOTSA_WITH_REST
  IotsaApiServiceRest restService;
#endif
#ifdef IOTSA_WITH_COAP
  IotsaApiServiceCoap coapService;
#endif
#ifdef IOTSA_WITH_HPS
  IotsaApiServiceHps bleRestService;
#endif
  int _dummy;
};

class IotsaApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early),
    api(this, _app, _auth)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
protected:
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    if (reqObj[name].is<JT>()) {
      var = reqObj[name].as<CT>();
      reqObj.remove(name);
      return true;
    }
    // IFDEBUG IotsaSerial.printf("xxxjack IotsaApi parameter %s not found\n", name);
    return false;
  }
  bool checkUnhandled(const JsonObject& reqObj) {
    bool rv = false;
    for (JsonPair kv : reqObj) {
      rv = true;
      IFDEBUG IotsaSerial.printf("Unhandled IotsaApi parameter: %s\n", kv.key().c_str());
    }
    return rv;
  }
  IotsaApiService api;
};

#endif // _IOTSAAPI_H_
