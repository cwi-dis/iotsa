#include "iotsaApi.h"

#ifdef IOTSA_WITH_WEB
void IotsaApiServiceWeb::setup(const char* path, bool get, bool put, bool post, bool webPage) {
    // A page doesn't have separate get/put/post variants the way REST/CoAP/HPS do --
    // get=true is simply "this path has a page"; put/post aren't currently used.
    // webPage lets a caller opt a specific path out of getting a page at all (e.g. a
    // collection's per-item sub-paths, which would otherwise each register a
    // byte-identical duplicate of the collection's own page) -- see cwi-dis/iotsa#217.
    if (get && webPage) {
        // Leaked deliberately: server->on() and the bound wrapper need this to
        // outlive this call, same reasoning as IotsaApiServiceRest.
        if (pagePath.length() == 0) pagePath = path;
        String *fullPath = new String(String("/") + path);
        // GET and POST explicitly: a page doesn't distinguish them (cwi-dis/iotsa#183),
        // and on(path, handler) without a method only means "any method" on the
        // WebServer/ESP8266WebServer. The HTTPS compat server registers just GET
        // there, so every POST form got a 404 over HTTPS
        // (jackjansen/esp32_idf5_https_server_compat#6).
        server->on(fullPath->c_str(), HTTP_GET, std::bind(&IotsaApiServiceWeb::_webHandlerWrapper, this));
        server->on(fullPath->c_str(), HTTP_POST, std::bind(&IotsaApiServiceWeb::_webHandlerWrapper, this));
    }
    if (next) next->setup(path, get, put, post, webPage);
}

void IotsaApiServiceWeb::_webHandlerWrapper() {
    iotsaNoKeepAlive(server);
    provider->webHandler();
}

void IotsaModule::webHandler() {
    IotsaWebContext ctx(*this, api.webService->server, "/api/" + api.webService->pagePath);
    formHandler_args(ctx);
    if (ctx.responded) return;
    if (ctx.configChanged) {
        configSave();
        iotsaController.extendCurrentMode();
        iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_MAGENTA, 0, 0, 2000, "Settings saved");  // cwi-dis/iotsa#176
        ctx.feedback("Settings saved.");
    } else if (ctx.stateChanged && !ctx.hasFeedback() && !ctx.failed()) {
        ctx.feedback("Done.");
    }
    ctx.reportUnused();
    ctx.setParts(true, false);
    ctx.resetRights();
    formHandler_fields(ctx);
    String everyday = ctx.takeOutput();
    ctx.setParts(false, true);
    ctx.resetRights();
    formHandler_fields(ctx);
    String settings = ctx.takeOutput();
    ctx.sendPage(webTitle(), everyday, settings);
}

void IotsaModule::addTrigger(const char *path, TriggerHandler handler, const char *right) {
    IotsaWebServer *server = api.webService->server;
    // Leaked deliberately, like the page's path in IotsaApiServiceWeb::setup().
    String *fullPath = new String(String("/") + path);
    auto wrapper = [this, server, handler, right]() {
        iotsaNoKeepAlive(server);
        // Auth like the module's page: the REST rule for its API path.
        IotsaWebContext ctx(*this, server, "/api/" + api.webService->pagePath);
        if (right) ctx.argsNeedRight(right);
        // A trigger is a change by itself.
        if (ctx._mayChange(IotsaContext::Transient)) handler(ctx);
        if (ctx.responded) return;
        ctx.reportUnused();
        ctx.sendText();
    };
    // GET and POST, for the same reason as a page.
    server->on(fullPath->c_str(), HTTP_GET, wrapper);
    server->on(fullPath->c_str(), HTTP_POST, wrapper);
}
#endif // IOTSA_WITH_WEB
