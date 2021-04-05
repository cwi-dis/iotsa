#ifndef _IOTSAREQUEST_H_
#define _IOTSAREQUEST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

class IotsaRequest : public IotsaApiModObject {
public:
  IotsaRequest() : url(""), sslInfo(""), credentials(""), token("") {}
  bool send(const char *query=NULL);
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
#ifdef IOTSA_WITH_WEB
  static void formHandler_new(String& message);
  void formHandler_body(String& message, const String& text, const String& f_name, bool includeConfig) override;
  static void formHandler_TH(String& message, bool includeConfig);
  void formHandler_TD(String& message, bool includeConfig) override;
  bool formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) override;
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