#ifndef _IOTSAHTTPSERVER_H_
#define _IOTSAHTTPSERVER_H_
#include "iotsa.h"

//
// The shared HTTP(S) transport: owns the actual IotsaWebServer instance and its
// setup/loop/not-found/root-page lifecycle. A peer service module -- the same
// tier as IotsaCoapServiceMod/IotsaHpsServiceMod -- rather than privileged
// IotsaApplication inheritance (see cwi-dis/iotsa#207, which replaced the old
// IotsaWebServerMixin with this).
//
// Unlike the CoAP/HPS companion mods, this one can't be created lazily on first
// use by whichever API-having module happens to construct first: IotsaBaseModule's
// constructor reads the shared `server` pointer for *every* module, API-having or
// not, so it has to already exist before any module (early or regular) is
// constructed. IotsaApplication's own constructor calls ensureServiceMod() to
// guarantee that, relying on the existing convention that the application object
// itself is declared before any module in the sketch.
//
// IotsaApiServiceWeb and IotsaApiServiceRest both reach the shared `server`
// through serviceMod(app)->server, instead of each holding their own copy --
// see cwi-dis/iotsa#211, which this only partially addresses: IotsaBaseModule's
// own per-module `server` field is deliberately left in place for now (still
// re-sourced from here rather than from IotsaApplication), full removal is
// #211's own, larger pass.
//
class IotsaHttpServiceMod : public IotsaBaseModule {
public:
  IotsaHttpServiceMod(IotsaApplication &_app);
  void setup() override;
  void serverSetup() override;
  void loop() override;
  static void ensureServiceMod(IotsaApplication &app);
  static IotsaHttpServiceMod *serviceMod(IotsaApplication &app);
  // The actual IotsaWebServer instance lives in the inherited IotsaBaseModule::server
  // field -- reused rather than duplicated, since every module (this one included)
  // already has that field. IotsaHttpServiceMod is the one module that assigns it
  // (in its own constructor) instead of just reading a copy of it. Re-exposed as
  // public here (it's protected on IotsaBaseModule) since IotsaApiServiceWeb/Rest,
  // unrelated classes, need to read it.
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  using IotsaBaseModule::server;
  bool serverInitialized = false;
#endif
private:
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
  static IotsaHttpServiceMod *_httpMod;
};

#endif
