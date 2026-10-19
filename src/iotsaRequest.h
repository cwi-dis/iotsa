#ifndef _IOTSAREQUEST_H_
#define _IOTSAREQUEST_H_
#include "iotsa.h"
#include "iotsaApi.h"
#include "iotsaConfigFile.h"

class IotsaRequest : public IotsaApiModObject {
public:
  IotsaRequest() : url(""), sslInfo(""), credentials(""), token("") {}
  // responseBody, if non-NULL, is filled in with the HTTP response body on a successful GET.
  //
  // Synchronous: blocks loop() until the reply is in. Only for servers that are
  // expected to answer quickly (e.g. another iotsa device). HTTPClient's default
  // timeouts apply, 5 s for the connect and 5 s without data, so a dead or slow
  // server costs up to about 10 s, and a server that trickles data can hold
  // loop() indefinitely. Calls that may legitimately take long need an
  // asynchronous variant (start the request, callbacks for reply and error),
  // which doesn't exist yet (cwi-dis/iotsa#279).
  bool send(const char *query=NULL, String *responseBody=NULL);
  bool configLoad(IotsaConfigFileLoad& cf, const String& f_name) override;
  void configSave(IotsaConfigFileSave& cf, const String& f_name) override;
#ifdef IOTSA_WITH_WEB
  // Credentials and token are secrets, as over REST: never shown, empty
  // leaves them unchanged, a "clear" checkbox empties them. The root CA (a
  // textarea) works the same way.
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
  void formHandler_row(IotsaWebContext& ctx) override;
#endif
  bool getHandler(JsonObject& reply) override;
  bool putHandler(const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override;
  String url;
  String sslInfo; // PEM root CA cert for https:// urls (both ESP32 and ESP8266)
  String credentials;
  String token;
};

#endif // _IOTSAREQUEST_H_