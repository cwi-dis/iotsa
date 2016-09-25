#ifndef _IOTSAFILES_H_
#define _IOTSAFILES_H_
#include "iotsa.h"

class IotsaFilesMod : public IotsaMod {
public:
  IotsaFilesMod(IotsaApplication &_app) : IotsaMod(_app) {}
  void setup();
  void serverSetup();
  void loop();
  String info();
private:
  void listHandler();
  void notFoundHandler();
};

#endif
