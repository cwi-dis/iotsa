#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
class IotsaFilesUploadMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
private:
  void uploadHandler();
  void uploadOkHandler();
  void uploadFormHandler();
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesUploadMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() {}
  void serverSetup() {}
  void loop() {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS

#endif
