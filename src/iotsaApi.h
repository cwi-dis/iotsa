#ifndef _IOTSAAPI_H_
#define _IOTSAAPI_H_
#include "iotsa.h"
#include <ArduinoJson.h>

class IotsaApiModObject : public IotsaModObject {
public:
  virtual ~IotsaApiModObject() {}

  // The same shape and rules as the module handlers, minus the path
  // (cwi-dis/iotsa#280): return false (after apiError() if you can say why) on
  // failure, true otherwise, also when nothing changed. changed, if given, is
  // set to true (never to false) when state changed that needs saving.
  virtual bool getHandler(JsonObject& reply) = 0;
  virtual bool putHandler(const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) = 0;
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    return iotsaGetFromRequest<JT>(reqObj, name, var);
  }
  static bool apiError(int status, const char *message) { return iotsaApiError(status, message); }
};

class IotsaApiServiceProvider {
public:
  IotsaApiServiceProvider(IotsaApiServiceProvider* _next=nullptr) : next(_next) {}
  virtual ~IotsaApiServiceProvider() {}
  // webPage: whether this call should also register a web page for `path` (only
  // consulted by IotsaApiServiceWeb; every other transport ignores it and just
  // forwards it down the chain). Defaults true so an ordinary api.setup() call keeps
  // registering a page whenever get=true, as before -- callers that register several
  // sub-paths of the same collection (buttons/N, users/N) pass webPage=false for the
  // per-item calls, since the module's webHandler() doesn't distinguish by path
  // anyway and a page per item would just be a byte-identical duplicate -- see
  // cwi-dis/iotsa#217.
  virtual void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) = 0;
protected:
  IotsaApiServiceProvider* next;
};

// Forward declaration, so IotsaApiService can expose a webService pointer (see below)
// even in builds without IOTSA_WITH_WEB, where the type is never fully defined and the
// pointer stays nullptr.
class IotsaApiServiceWeb;

#ifdef IOTSA_HAS_RESTSERVER
#include "iotsaApiRest.h"
#endif
#ifdef IOTSA_HAS_COAPSERVER
#include "iotsaApiCoap.h"
#endif
#ifdef IOTSA_HAS_HPSSERVER
#include "iotsaApiHps.h"
#endif
#ifdef IOTSA_WITH_WEB
#include "iotsaApiWeb.h"
#include "iotsaWebContext.h"
#endif

//
// Chains together whichever of the REST/CoAP/HPS/Web transport services are
// compiled in, each forwarding setup() to the next link. Collapses what used
// to be three ifdef'd members/init-list entries/setup() calls down to one
// ifdef'd chain-building constructor, see cwi-dis/iotsa#213.
//
// Web is a link like any other (a single api.setup() call registers the REST/CoAP/HPS
// endpoint(s) *and* the module's web page, when get=true and webPage isn't turned off)
// -- see cwi-dis/iotsa#213. A module's webHandler() body reaches the shared HTTP
// server through `webService` (e.g. `api.webService->server`), instead of holding a
// `server` member of its own --
// see cwi-dis/iotsa#211. A module that also needs something a bare setup() call can't
// express, and that isn't really part of its own page (a not-found/wildcard route, a
// two-callback upload handler -- see IotsaConfigMod's cert upload, cwi-dis/iotsa#221),
// reaches app.server directly instead, the same way web-server-extension modules do.
//
class IotsaApiService : public IotsaApiServiceProvider {
public:
  IotsaApiService(IotsaApiProvider* _provider, IotsaApplication &_app) {
  #ifdef IOTSA_WITH_WEB
    next = webService = new IotsaApiServiceWeb(_provider, _app, next);
  #endif
  #ifdef IOTSA_HAS_HPSSERVER
    next = new IotsaApiServiceHps(_provider, _app, next);
  #endif
  #ifdef IOTSA_HAS_COAPSERVER
    next = new IotsaApiServiceCoap(_provider, _app, next);
  #endif
  #ifdef IOTSA_HAS_RESTSERVER
    next = new IotsaApiServiceRest(_provider, _app, next);
  #endif
  }
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override {
    if (next) next->setup(path, get, put, post, webPage);
  }
  IotsaApiServiceWeb* webService = nullptr;
};

class IotsaModule : public IotsaBaseModule {
public:
  IotsaModule(IotsaApplication &_app, bool early=false)
  : IotsaBaseModule(_app, early),
    api(this, _app)
  {}
  // See IotsaBaseModule: deleted so old code passing an auth provider fails to compile.
  IotsaModule(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool early=false) = delete;
  virtual bool getHandler(const char *path, JsonObject& reply) override { return false; }
  virtual bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override { return false; }
  virtual bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed = nullptr) override { return false; }
  bool hasApi() const override { return true; }
#ifdef IOTSA_WITH_WEB
  // The default page (cwi-dis/iotsa#183): formHandler_args() handles the
  // arguments, the framework saves the configuration when a Config field
  // changed, then formHandler_fields() is called once per section (day-to-day,
  // configuration) to fill the page. Override webHandler() itself only for a
  // page that doesn't fit (streaming, a text interface).
  void webHandler() override;
  virtual String webTitle() { return name; }
  virtual void formHandler_args(IotsaWebContext& ctx) {}
  virtual void formHandler_fields(IotsaWebContext& ctx) {}
#endif
protected:
#ifdef IOTSA_WITH_WEB
  // A trigger (cwi-dis/iotsa#183): a plain URL that does something right away,
  // for curl or another device, like GET /ring?alarm=20. The handler reads the
  // arguments with ctx.arg() (kind Live), as formHandler_args() does. Every
  // call is a change, so the right is checked before the handler runs: the
  // REST rule for the module's API path, or the named right if given. The
  // answer is plain text: the ctx.feedback() lines, or "OK"; 400 with the
  // ctx.error() lines. Call it in lateSetup(), next to api.setup().
  typedef std::function<void(IotsaWebContext& ctx)> TriggerHandler;
  void addTrigger(const char *path, TriggerHandler handler, const char *right = nullptr);
#endif
  // Like the other getFromRequest() copies, but also removes a consumed field,
  // so checkUnhandled() can report whatever is left over.
  template <typename JT, typename CT>  bool getFromRequest(const JsonObject& reqObj, const char *name, CT& var) {
    if (!iotsaGetFromRequest<JT>(reqObj, name, var)) {
      // IFDEBUG IotsaSerial.printf("xxxjack IotsaApi parameter %s not found\n", name);
      return false;
    }
    reqObj.remove(name);
    return true;
  }
  // Reports the fields still in reqObj (the ones getFromRequest() didn't consume)
  // as "iotsa_api_ignored" in the reply (cwi-dis/iotsa#280). Returns true if any.
  bool checkUnhandled(const JsonObject& reqObj) {
    bool rv = false;
    for (JsonPair kv : reqObj) {
      rv = true;
      iotsaApiResult.ignored.push_back(kv.key().c_str());
    }
    return rv;
  }
  IotsaApiService api;
};

// An authentication provider is a module with an API (its users, its
// capabilities), so it is an IotsaModule: it gets the shared api and the
// default web page (cwi-dis/iotsa#183).
class IotsaAuthMod : public IotsaModule, public IotsaAuthenticationProvider {
public:
  IotsaAuthMod(IotsaApplication &_app, bool early=false)
  : IotsaModule(_app, early)
  {
    _app._haveAuthModule = true;
  }
  IotsaAuthMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth, bool early=false) = delete;
};

#endif // _IOTSAAPI_H_
