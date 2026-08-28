#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
class IotsaFilesUploadMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
private:
  void uploadHandler();
  void uploadOkHandler();
  void uploadFormHandler();
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesUploadMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS

#endif
