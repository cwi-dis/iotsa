#include "iotsaCoapApi.h"

void IotsaCoapApiService::setup(const char* path, bool get, bool put, bool post) {
#if 0
    if (get) server.on(path, HTTP_GET, std::bind(&IotsaApi::_getHandlerWrapper, this, path));
    if (put) server.on(path, HTTP_PUT, std::bind(&IotsaApi::_putHandlerWrapper, this, path));
    if (post) server.on(path, HTTP_POST, std::bind(&IotsaApi::_postHandlerWrapper, this, path));
#endif
}
