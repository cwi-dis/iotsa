#include "iotsaApi.h"

#ifdef IOTSA_WITH_REST
void IotsaApiServiceRest::setup(const char* path, bool get, bool put, bool post) {
    // xxxjack may be enabled later... if (!iotsaConfig.wifiEnabled) return;
    if (get) server->on(path, HTTP_GET, std::bind(&IotsaApiServiceRest::_getHandlerWrapper, this, path));
    if (put) server->on(path, HTTP_PUT, std::bind(&IotsaApiServiceRest::_putHandlerWrapper, this, path));
    if (post) server->on(path, HTTP_POST, std::bind(&IotsaApiServiceRest::_postHandlerWrapper, this, path));
}

void IotsaApiServiceRest::_getHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_GET)) return;
    IFDEBUG IotsaSerial.print("GET api ");
    IFDEBUG IotsaSerial.println(path);
    iotsaConfig.postponeSleep(0);
    int jsonBufSize = 2048;
    if (server->hasArg("jsonBufSize")) {
        jsonBufSize = server->arg("jsonBufSize").toInt();
    }
    DynamicJsonDocument replyDocument(jsonBufSize);
    JsonObject reply = replyDocument.to<JsonObject>();
    bool ok = provider->getHandler(path, reply);
    if (replyDocument.overflowed()) {
        server->send(413, "text/plain", "JSON document overflow, use jsonBufSize");
        IFDEBUG IotsaSerial.println("-> ERR JSON document overflow");
        return;
    }
    if (ok) {
        String replyData;
        serializeJson(replyDocument, replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR bad request");
    }
}

void IotsaApiServiceRest::_putHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_PUT)) return;
    IFDEBUG IotsaSerial.print("PUT api ");
    IFDEBUG IotsaSerial.println(path);
    iotsaConfig.postponeSleep(0);
    int jsonBufSize = 2048;
    if (server->hasArg("jsonBufSize")) {
        jsonBufSize = server->arg("jsonBufSize").toInt();
    }
    DynamicJsonDocument replyDocument(jsonBufSize);
    JsonObject reply = replyDocument.to<JsonObject>();
    DynamicJsonDocument requestDocument(jsonBufSize);
    deserializeJson(requestDocument, server->arg("plain"));
    if (requestDocument.overflowed()) {
        server->send(413, "text/plain", "JSON request overflow, use jsonBufSize");
        IFDEBUG IotsaSerial.println("-> ERR JSON request overflow");
        return;
    }
    JsonObject request = requestDocument.as<JsonObject>();
    bool ok = provider->putHandler(path, request, reply);
    if (replyDocument.overflowed()) {
        server->send(413, "text/plain", "JSON reply overflow, use jsonBufSize");
        IFDEBUG IotsaSerial.println("-> ERR JSON reply overflow");
        return;
    }
    if (ok) {
        String replyData;
        serializeJson(replyDocument, replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR bad request");
    }
}

void IotsaApiServiceRest::_postHandlerWrapper(const char *path) {
    if (auth && !auth->allows(path, IOTSA_API_POST)) return;
    IFDEBUG IotsaSerial.print("POST api ");
    IFDEBUG IotsaSerial.println(path);
    iotsaConfig.postponeSleep(0);
    int jsonBufSize = 2048;
    if (server->hasArg("jsonBufSize")) {
        jsonBufSize = server->arg("jsonBufSize").toInt();
    }
    DynamicJsonDocument replyDocument(jsonBufSize);
    JsonObject reply = replyDocument.to<JsonObject>();
    DynamicJsonDocument requestDocument(jsonBufSize);
    deserializeJson(requestDocument, server->arg("plain"));
    if (requestDocument.overflowed()) {
        server->send(413, "text/plain", "JSON document overflow, use jsonBufSize");
        IFDEBUG IotsaSerial.println("-> ERR JSON document overflow");
        return;
    }
    JsonObject request = requestDocument.as<JsonObject>();
    bool ok = provider->postHandler(path, request, reply);
    if (replyDocument.overflowed()) {
        server->send(413, "text/plain", "JSON document overflow, use jsonBufSize");
        IFDEBUG IotsaSerial.println("-> ERR JSON document overflow");
        return;
    }
    if (ok) {
        String replyData;
        serializeJson(replyDocument, replyData);
        server->send(200, "application/json", replyData);
        IFDEBUG IotsaSerial.println("-> OK");
    } else {
        server->send(400, "text/plain", "\"bad request\"");
        IFDEBUG IotsaSerial.println("-> ERR");
    }
}
#endif // IOTSA_WITH_REST