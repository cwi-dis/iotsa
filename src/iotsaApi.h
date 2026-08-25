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

#ifdef IOTSA_WITH_API
// Specialization, so that bools can be retrieved from int fields as well
template<> inline bool IotsaApiModObject::getFromRequest<bool,bool>(const JsonObject& reqObj, const char *name, bool& var) {
  if (reqObj[name].is<bool>()) {
    var = reqObj[name].as<bool>();
    return true;
  }
  if (reqObj[name].is<int>()) {
    int ival = reqObj[name].as<int>();
    var = (ival != 0);
    return true;
  }
  return false;
}
#endif

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

class IotsaModule : public IotsaBaseModule {
public:
  IotsaModule(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
  : IotsaBaseModule(_app, _auth, early),
    api(this, _app, _auth)
  {}
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) override { return false; }
  bool hasApi() const override { return true; }
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
