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
        server->on(fullPath->c_str(), std::bind(&IotsaApiServiceWeb::_webHandlerWrapper, this));
    }
    if (next) next->setup(path, get, put, post, webPage);
}

void IotsaApiServiceWeb::_webHandlerWrapper() {
    provider->webHandler();
}
#endif // IOTSA_WITH_WEB
