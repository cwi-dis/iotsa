#ifndef _IOTSALOGGER_H_
#define _IOTSALOGGER_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_HTTP_OR_HTTPS
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
class IotsaLoggerMod : public IotsaBaseModule {
public:
  IotsaLoggerMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL);
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  void configLoad() override;
  void configSave() override;
  void handler();
  String argument;
};
#endif // IOTSA_WITH_HTTP_OR_HTTPS
#endif
