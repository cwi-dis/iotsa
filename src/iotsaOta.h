#ifndef _IOTSAOTA_H_
#define _IOTSAOTA_H_
#include "iotsa.h"

class IotsaOtaMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
  String info() override;
};

#endif
