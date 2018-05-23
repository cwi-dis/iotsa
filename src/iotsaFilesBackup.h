#ifndef _IOTSAFILESBACKUP_H_
#define _IOTSAFILESBACKUP_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
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
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesBackupMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() {}
  void serverSetup() {}
  void loop() {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS

#endif
