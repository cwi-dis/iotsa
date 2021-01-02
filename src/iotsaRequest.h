#ifndef _IOTSAREQUEST_H_
#define _IOTSAREQUEST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

class IotsaRequest : IotsaApiModObject {
public:
  IotsaRequest() : url(""), sslInfo(""), credentials(""), token("") {}
  bool send();
  bool configLoad(IotsaConfigFileLoad& cf, String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, String& f_name) override;
#ifdef IOTSA_WITH_WEB
  void formHandler(String& message, String& text, String& f_name) override;
  void formHandlerTD(String& message) override;
  bool formArgHandler(IotsaWebServer *server, String f_name) override;
#endif
#ifdef IOTSA_WITH_API
  void getHandler(JsonObject& reply) override;
  bool putHandler(const JsonVariant& request) override;
#endif
  String url;
  String sslInfo;
  String credentials;
  String token;
};

#endif // _IOTSAREQUEST_H_