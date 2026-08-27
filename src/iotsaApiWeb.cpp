#include "iotsaApi.h"

#ifdef IOTSA_WITH_WEB
void IotsaApiServiceWeb::setup(const char* path, bool get, bool put, bool post) {
    // A page doesn't have separate get/put/post variants the way REST/CoAP/HPS do --
    // get=true is simply "this path has a page"; put/post aren't currently used.
    if (get) {
        // Leaked deliberately: server->on() and the bound wrapper need this to
        // outlive this call, same reasoning as IotsaApiServiceRest.
        String *fullPath = new String(String("/") + path);
        server->on(fullPath->c_str(), std::bind(&IotsaApiServiceWeb::_webHandlerWrapper, this));
    }
    if (next) next->setup(path, get, put, post);
}

void IotsaApiServiceWeb::_webHandlerWrapper() {
    provider->webHandler();
}
#endif // IOTSA_WITH_WEB
