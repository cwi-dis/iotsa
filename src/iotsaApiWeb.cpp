#include "iotsaApi.h"

#ifdef IOTSA_WITH_WEB
void IotsaWebServiceProvider::setup(const char* path, bool get, bool put, bool post) {
    // A page doesn't have separate get/put/post variants the way REST/CoAP/HPS do --
    // every module today registers its page for any HTTP method (either via a single
    // implicit ANY-method server->on(uri, handler) call, or a redundant explicit
    // GET+POST pair calling the same handler either way). get=true is simply "this
    // path has a page"; put/post aren't currently used.
    if (get) {
        // Leaked deliberately: server->on() and the bound wrapper need this to
        // outlive this call, same reasoning as IotsaApiServiceRest.
        String *fullPath = new String(String("/") + path);
        server->on(fullPath->c_str(), std::bind(&IotsaWebServiceProvider::_webHandlerWrapper, this));
    }
}

void IotsaWebServiceProvider::_webHandlerWrapper() {
    provider->webHandler();
}
#endif // IOTSA_WITH_WEB
