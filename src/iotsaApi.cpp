#include "iotsaApi.h"

void IotsaApiMod::apiSetup(String path, bool get, bool put, bool post) {
    if (get) server.on(path, std::bind(&IotsaApiMod::_getHandlerWrapper, this));
    if (put) server.on(path, HTTP_PUT, std::bind(&IotsaApiMod::_putHandlerWrapper, this));
    if (post) server.on(path, HTTP_POST, std::bind(&IotsaApiMod::_postHandlerWrapper, this));
}

void IotsaApiMod::_getHandlerWrapper() {
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = getHandler(reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
    } else {
        server.send(400, "text/plain", "\"bad request\"");
    }
}

void IotsaApiMod::_putHandlerWrapper() {
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = putHandler(request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
    } else {
        server.send(400, "text/plain", "\"bad request\"");
    }
}

void IotsaApiMod::_postHandlerWrapper() {
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = postHandler(request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
    } else {
        server.send(400, "text/plain", "\"bad request\"");
    }
}
