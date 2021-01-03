#ifndef _IOTSAOTA_H_
#define _IOTSAOTA_H_
#include "iotsa.h"

class IotsaOtaMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() override;
  void serverSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
};

#endif
