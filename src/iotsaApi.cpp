#include "iotsaApi.h"

void IotsaApiMod::apiSetup(const char* path, bool get, bool put, bool post) {
    if (get) server.on(path, HTTP_GET, std::bind(&IotsaApiMod::_getHandlerWrapper, this, path));
    if (put) server.on(path, HTTP_PUT, std::bind(&IotsaApiMod::_putHandlerWrapper, this, path));
    if (post) server.on(path, HTTP_POST, std::bind(&IotsaApiMod::_postHandlerWrapper, this, path));
}

void IotsaApiMod::_getHandlerWrapper(const char *path) {
    IFDEBUG IotsaSerial.print("GET api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = getHandler(path, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server.send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}

void IotsaApiMod::_putHandlerWrapper(const char *path) {
    IFDEBUG IotsaSerial.print("PUT api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = putHandler(path, request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server.send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}

void IotsaApiMod::_postHandlerWrapper(const char *path) {
    IFDEBUG IotsaSerial.print("POST api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = postHandler(path, request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server.send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server.send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}
