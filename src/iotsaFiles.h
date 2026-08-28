#ifndef _IOTSAFILES_H_
#define _IOTSAFILES_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
class IotsaFilesMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  virtual bool accessAllowed(String &path);	// Return true if allowed, default only for /data/*
private:
  void listHandler();
  void notFoundHandler();
  void _listDir(String& message, const char *name);
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS
#endif
