#ifndef _IOTSAREQUEST_H_
#define _IOTSAREQUEST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

class IotsaRequest {
public:
  IotsaRequest() : url(""), sslInfo(""), credentials(""), token("") {}
  bool send();
  void configLoad(IotsaConfigFileLoad& cf, String& name);
  void configSave(IotsaConfigFileSave& cf, String& name);
  void formHandler(String& message, String& text, String& name);
#ifdef IOTSA_WITH_WEB
  bool formArgHandler(IotsaWebServer *server, String name);
#endif
#ifdef IOTSA_WITH_API
  void getHandler(JsonObject& reply);
  bool putHandler(const JsonVariant& request);
#endif
  String url;
  String sslInfo;
  String credentials;
  String token;
};

#endif // _IOTSAREQUEST_H_