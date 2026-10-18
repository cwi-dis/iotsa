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
#endif // IOTSA_WITH_WEB
