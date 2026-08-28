#ifndef _IOTSAFILESBACKUP_H_
#define _IOTSAFILESBACKUP_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
class IotsaFilesBackupMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
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
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS

#endif
