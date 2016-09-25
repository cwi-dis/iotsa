#ifndef _IOTSAFILESUPLOAD_H_
#define _IOTSAFILESUPLOAD_H_
#include "iotsa.h"

class IotsaFilesUploadMod : public IotsaMod {
public:
  IotsaFilesUploadMod(IotsaApplication &_app) : IotsaMod(_app) {}
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
