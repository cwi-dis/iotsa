#ifndef _IOTSAFILESBACKUP_H_
#define _IOTSAFILESBACKUP_H_
#include "iotsa.h"

class IotsaFilesBackupMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
private:
  void handler();
};

#endif
