#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiProvider {
public:
  IotsaApiProvider() {}
  virtual ~IotsaApiProvider() {}
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool needsAuthentication(const char *right, const char *verb) {return false;}
};

class IotsaApi {
public:
  IotsaApi(IotsaApiProvider* _provider, IotsaWebServer& _server)
  : provider(_provider),
    server(_server)
  {}
  void setup(const char* path, bool get=false, bool put=false, bool post=false);
private:
  IotsaApiProvider* provider; 
  IotsaWebServer& server;
  void _getHandlerWrapper(const char *path);
  void _putHandlerWrapper(const char *path);
  void _postHandlerWrapper(const char *path);
};

class IotsaApiMod : public IotsaMod, public IotsaApiProvider {
public:
  IotsaApiMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false)
  : IotsaMod(_app, _auth, early),
    api(this, server)
  {}
protected:
  IotsaApi api;
};
#endif
