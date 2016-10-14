#ifndef _IOTSAOTA_H_
#define _IOTSAOTA_H_
#include "iotsa.h"

class IotsaOtaMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
};

#endif
