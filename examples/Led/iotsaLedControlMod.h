#ifndef _IOTSALEDCONTROLMOD_H_
#define _IOTSALEDCONTROLMOD_H_

#include "iotsa.h"
#include "iotsaLed.h"

//
// LED module.
//
class IotsaLedControlMod : public IotsaLedMod {
public:
  using IotsaLedMod::IotsaLedMod;
  void lateSetup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
private:
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
};

#endif
