#ifndef _IOTSAOTA_H_
#define _IOTSAOTA_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_OTA
// Created automatically by IotsaApplication::setup() (cwi-dis/iotsa#85); an
// explicit declaration in a sketch is still allowed and then used instead.
class IotsaOtaMod : public IotsaBaseModule, public IotsaSingletonModule<IotsaOtaMod> {
public:
  IotsaOtaMod(IotsaApplication &_app)
  : IotsaBaseModule(_app)
  {
    claimSingleton(this);
  }
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool _started = false;
  void _startIfReady();
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaOtaMod : public IotsaBaseModule {
public:
  using IotsaBaseModule::IotsaBaseModule;
  void setup() override {}
  void lateSetup() override {}
  void loop() override {}
  String info() override {return "";}
};
#endif // IOTSA_WITH_OTA || IOTSA_WITH_PLACEHOLDERS

#endif
