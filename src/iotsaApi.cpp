#include "iotsaApi.h"

void IotsaApi::setup(const char* path, bool get, bool put, bool post) {
    if (get) server.on(path, HTTP_GET, std::bind(&IotsaApi::_getHandlerWrapper, this, path));
    if (put) server.on(path, HTTP_PUT, std::bind(&IotsaApi::_putHandlerWrapper, this, path));
    if (post) server.on(path, HTTP_POST, std::bind(&IotsaApi::_postHandlerWrapper, this, path));
}

void IotsaApi::_getHandlerWrapper(const char *path) {
    if (provider->needsAuthentication(path, "get")) return;
    IFDEBUG IotsaSerial.print("GET api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->getHandler(path, reply);
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

void IotsaApi::_putHandlerWrapper(const char *path) {
    if (provider->needsAuthentication(path, "put")) return;
    IFDEBUG IotsaSerial.print("PUT api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->putHandler(path, request, reply);
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

void IotsaApi::_postHandlerWrapper(const char *path) {
    if (provider->needsAuthentication(path, "post")) return;
    IFDEBUG IotsaSerial.print("POST api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server.arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->postHandler(path, request, reply);
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
