#ifndef _IOTSAFILES_H_
#define _IOTSAFILES_H_
#include "iotsa.h"

#ifdef IOTSA_HAS_WEBSERVER
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
//
// notFoundHandler() serves raw static file content (or a plain-text 404), which
// has nothing to do with a web UI -- only needs an HTTP transport
// (IOTSA_HAS_WEBSERVER), so a REST-only build can still serve files. Only info()
// is IOTSA_WITH_WEB-gated -- see cwi-dis/iotsa#205. listHandler()/_listDir()
// (the /data directory listing) render HTML unconditionally, a known minor wart
// left as-is here (a REST-only build gets one incidental HTML page) rather than
// inventing a JSON listing format as a side effect of this fix.
class IotsaFilesMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
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
#endif // IOTSA_HAS_WEBSERVER || IOTSA_WITH_PLACEHOLDERS
#endif
