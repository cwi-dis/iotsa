#ifndef _IOTSAREQUEST_H_
#define _IOTSAREQUEST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

class IotsaRequest : IotsaApiModObject {
public:
  IotsaRequest() : url(""), sslInfo(""), credentials(""), token("") {}
  bool send();
  bool configLoad(IotsaConfigFileLoad& cf, String& f_name);
  void configSave(IotsaConfigFileSave& cf, String& f_name);
#ifdef IOTSA_WITH_WEB
  void formHandler(String& message, String& text, String& f_name);
  void formHandlerTD(String& message);
  bool formArgHandler(IotsaWebServer *server, String f_name);
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