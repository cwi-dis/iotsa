#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

#ifdef IOTSA_HAS_WEBSERVER
// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
//
// The raw multipart upload itself only needs an HTTP transport (IOTSA_HAS_WEBSERVER),
// not a rendered web UI -- a REST-only, no-web-UI build should still be able to
// upload a file. Only uploadFormHandler()/info() (the HTML upload form and its page
// link) are gated on IOTSA_WITH_WEB specifically -- see cwi-dis/iotsa#205.
class IotsaFilesUploadMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
private:
  void uploadHandler();
  void uploadOkHandler();
#ifdef IOTSA_WITH_WEB
  void uploadFormHandler();
#endif
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesUploadMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
};
#endif // IOTSA_HAS_WEBSERVER || IOTSA_WITH_PLACEHOLDERS

#endif
