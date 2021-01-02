#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
class IotsaFilesUploadMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
private:
  void uploadHandler();
  void uploadOkHandler();
  void uploadFormHandler();
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesUploadMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() override {}
  void serverSetup() override {}
  void loop() override {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS

#endif
