#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiProvider  {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  virtual bool getHandler(const char *path, JsonObject& reply) = 0;
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) = 0;
};

class IotsaApiService {
public:
  IotsaApiService() {}
  virtual ~IotsaApiService() {}
  virtual void setup(const char* path, bool get=false, bool put=false, bool post=false) = 0;
};

class IotsaRestApiService : public IotsaApiService {
public:
  IotsaRestApiService(IotsaApiProvider* _provider, IotsaAuthenticationProvider* _auth, IotsaWebServer& _server)
  : provider(_provider),
    auth(_auth),
    server(_server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false);
private:
  IotsaApiProvider* provider; 
  IotsaAuthenticationProvider* auth;
  IotsaWebServer& server;
  void _getHandlerWrapper(const char *path);
  void _putHandlerWrapper(const char *path);
  void _postHandlerWrapper(const char *path);
};

class IotsaApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaApiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL, bool early=false)
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
