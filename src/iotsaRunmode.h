#ifndef _IOTSARUNMODE_H_
#define _IOTSARUNMODE_H_
#include "iotsa.h"
#include "iotsaApi.h"

//
// IotsaRunmodeMod -- the external control surface onto IotsaController
// (cwi-dis/iotsa#106, docs/controller-architecture.md).
//
// Core-tier: unconditionally ensure()d by IotsaApplication::setup(), the same
// treatment as IotsaConfigMod -- never an optional add. It is the one place,
// across every transport (REST/web now, BLE next), that a client steers the
// device's *operating state*: request a maintenance mode for the next boot,
// reboot, toggle the WiFi / BLE radios at runtime.
//
// Every handler here is thin glue: a call into iotsaController. The mode /
// reboot / radio keys also still appear in /api/config as [[deprecated]]
// forwarders for one release, so existing scripts and the Python CLI keep
// working until they move to /api/runmode (see the "Transition strategy"
// section of docs/controller-architecture.md).
//
class IotsaRunmodeMod : public IotsaModule, public IotsaSingletonModule<IotsaRunmodeMod> {
public:
  IotsaRunmodeMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaModule(_app, _auth, true)   // early: mode/reboot control belongs up before the app modules
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
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
};

#endif
