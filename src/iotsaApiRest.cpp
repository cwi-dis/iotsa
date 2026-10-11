#include "iotsaApi.h"

#ifdef IOTSA_HAS_RESTSERVER
void IotsaApiServiceRest::setup(const char* path, bool get, bool put, bool post, bool webPage) {
    // xxxjack may be enabled later... if (!iotsaConfig.wifiEnabled) return;
    // Callers pass a bare name; REST is the one that cares about the /api/ prefix, both
    // for its own HTTP registration and for what it hands to the module's handlers, so it
    // builds (and keeps, since server->on() and the bound wrappers need it to outlive this
    // call) its own permanent copy rather than relying on the caller's path to persist.
    String *fullPath = new String(String("/api/") + path);
    const char *p = fullPath->c_str();
    if (get) server->on(p, HTTP_GET, std::bind(&IotsaApiServiceRest::_handle, this, IOTSA_API_GET, p));
    if (put) server->on(p, HTTP_PUT, std::bind(&IotsaApiServiceRest::_handle, this, IOTSA_API_PUT, p));
    if (post) server->on(p, HTTP_POST, std::bind(&IotsaApiServiceRest::_handle, this, IOTSA_API_POST, p));
    // webPage is Web-only; REST ignores it and just forwards it down the chain.
    if (next) next->setup(path, get, put, post, webPage);
}

void IotsaApiServiceRest::_handle(IotsaApiOperation verb, const char *path) {
    IotsaActivityScope activity(IOTSA_CRUMB_REQUEST, IOTSA_CRUMB_REST);
    iotsaApiBegin("rest", verb == IOTSA_API_GET ? "GET" : verb == IOTSA_API_PUT ? "PUT" : "POST", path);
    JsonDocument replyDocument;
    JsonObject reply = replyDocument.to<JsonObject>();
    bool ok = false;
    if (!provider->needsAuthentication(path, verb)) {
        iotsaController.noteActivity();
        if (verb == IOTSA_API_GET) {
            ok = provider->getHandler(path, reply);
        } else {
            JsonDocument requestDocument;
            DeserializationError err = deserializeJson(requestDocument, server->arg("plain"));
            if (err == DeserializationError::NoMemory || requestDocument.overflowed()) {
                iotsaApiError(413, "request too big");
            } else if (err && err != DeserializationError::EmptyInput) {
                iotsaApiError(400, "invalid JSON");
            } else {
                JsonObject request = requestDocument.as<JsonObject>();
                ok = verb == IOTSA_API_PUT ? provider->putHandler(path, request, reply) : provider->postHandler(path, request, reply);
            }
        }
    }
    String body;
    int status = iotsaApiEnd(ok, replyDocument, body);
    // A refusing authenticator has already sent its own reply (a 401 challenge).
    if (iotsaApiResult.authResponded) return;
    server->send(status, "application/json", body);
}
#endif // IOTSA_HAS_RESTSERVER