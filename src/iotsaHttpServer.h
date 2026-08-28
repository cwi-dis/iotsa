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
// use by whichever module happens to need it: several categories of module reach
// for it during their own construction (web-server-extension modules) or need it
// to exist before any module's constructor runs. IotsaApplication's own constructor
// calls ensureServiceMod() to guarantee that, relying on the existing convention
// that the application object itself is declared before any module in the sketch.
//
// IotsaBaseModule has no `server` field of its own (see cwi-dis/iotsa#211) --
// this is the one, single owner. Callers reach it as follows:
//  - API-having modules' webHandler() bodies (get/put/postHandler + webHandler())
//    go through their own IotsaApiServiceWeb link, e.g. `api.webService->server`.
//  - Everyone else -- web-server-extension modules that are HTTP by nature rather
//    than multi-transport API providers (IotsaFilesUploadMod, IotsaFilesBackupMod,
//    IotsaFilesMod, IotsaLoggerMod, IotsaSimpleMod), a module's own registrations
//    that fall outside its page (IotsaConfigMod's cert upload, cwi-dis/iotsa#221),
//    the three auth-provider modules (IotsaUserMod/IotsaMultiUserMod/
//    IotsaCapabilityMod, a known wart pending cwi-dis/iotsa#107's context-object
//    redesign), and app-level sketch code -- reads IotsaApplication::server, which
//    is populated from this same object once in IotsaApplication's constructor.
//  - IotsaApiServiceRest/Web themselves reach it via serviceMod(app)->server.
//
class IotsaHttpServiceMod : public IotsaBaseModule {
public:
  IotsaHttpServiceMod(IotsaApplication &_app);
  void setup() override;
  void lateSetup() override;
  void loop() override;
  static void ensureServiceMod(IotsaApplication &app);
  static IotsaHttpServiceMod *serviceMod(IotsaApplication &app);
  // The actual IotsaWebServer instance. IotsaBaseModule no longer has a `server`
  // field of its own (see cwi-dis/iotsa#211) -- this is the one, single owner; see
  // the comment above for how the rest of the code reaches it.
#ifdef IOTSA_HAS_WEBSERVER
  IotsaWebServer *server = nullptr;
  bool serverInitialized = false;
#endif
private:
#ifdef IOTSA_HAS_WEBSERVER
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
  static IotsaHttpServiceMod *_httpMod;
};

#endif
