#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiMod : public IotsaBaseMod {
public:
  using IotsaMod::IotsaMod;
protected:
  void apiSetup(String path, bool get=false, bool put=false, bool post=false, bool delete=false);
  virtual bool getHandler(const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool putHandler(const JsonVariant& request, JsonObject& reply) { return false; }
  virtual bool postHandler(const JsonVariant& request, JsonObject& reply) { return false; }
private:
  void _getHandlerWrapper();
  void _putHandlerWrapper();
  void _postHandlerWrapper();
};

#endif
