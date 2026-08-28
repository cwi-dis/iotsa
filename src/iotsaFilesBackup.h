#ifndef _IOTSAFILESBACKUP_H_
#define _IOTSAFILESBACKUP_H_
#include "iotsa.h"

#ifdef IOTSA_HAS_WEBSERVER
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
//
// handler() streams a raw tar archive, no HTML involved -- only needs an HTTP
// transport (IOTSA_HAS_WEBSERVER), not a rendered web UI, so a REST-only build
// still gets backups. Only info() (the page link) is IOTSA_WITH_WEB-gated --
// see cwi-dis/iotsa#205.
class IotsaFilesBackupMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
private:
  void handler();
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesBackupMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
};
#endif // IOTSA_HAS_WEBSERVER || IOTSA_WITH_PLACEHOLDERS

#endif
