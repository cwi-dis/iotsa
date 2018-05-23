#include "iotsaApi.h"

#ifdef IOTSA_WITH_REST
void IotsaRestApiService::setup(const char* path, bool get, bool put, bool post) {
    if (get) server->on(path, HTTP_GET, std::bind(&IotsaRestApiService::_getHandlerWrapper, this, path));
    if (put) server->on(path, HTTP_PUT, std::bind(&IotsaRestApiService::_putHandlerWrapper, this, path));
    if (post) server->on(path, HTTP_POST, std::bind(&IotsaRestApiService::_postHandlerWrapper, this, path));
}

void IotsaRestApiService::_getHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_GET)) return;
    IFDEBUG IotsaSerial.print("GET api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->getHandler(path, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}

void IotsaRestApiService::_putHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_PUT)) return;
    IFDEBUG IotsaSerial.print("PUT api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server->arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->putHandler(path, request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}

void IotsaRestApiService::_postHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_POST)) return;
    IFDEBUG IotsaSerial.print("POST api ");
    IFDEBUG IotsaSerial.println(path);
    DynamicJsonBuffer requestBuffer;
    JsonObject& request = requestBuffer.parseObject(server->arg("plain"));
    DynamicJsonBuffer replyBuffer;
    JsonObject& reply = replyBuffer.createObject();
    bool ok = provider->postHandler(path, request, reply);
    if (ok) {
        String replyData;
        reply.printTo(replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}
#endif // IOTSA_WITH_REST