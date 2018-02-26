#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
protected:
  void apiSetup(const char* path, bool get=false, bool put=false, bool post=false);
  virtual bool getHandler(const char *path, JsonObject& reply) { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply) { return false; }
private:
  void _getHandlerWrapper(const char *path);
  void _putHandlerWrapper(const char *path);
  void _postHandlerWrapper(const char *path);
};

#endif
