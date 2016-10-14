#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

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

#endif
