#include "iotsaApi.h"

void IotsaApiMod::apiSetup(String path, bool get=false, bool put=false, bool post=false) {
    if (get) server.on(path, std::bind(&IotsaApiMod::_getHandlerWrapper, this));
    if (put) server.on(path, HTTP_PUT, std::bind(&IotsaApiMod::_putHandlerWrapper, this));
    if (post) server.on(path, HTTP_POST, std::bind(&IotsaApiMod::_postHandlerWrapper, this));
}

void IotsaApiMod::_getHandlerWrapper() {

}

void IotsaApiMod::_putHandlerWrapper() {

}

void IotsaApiMod::_postHandlerWrapper() {

}
