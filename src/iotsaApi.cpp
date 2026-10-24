#include "iotsa.h"
#include "iotsaApi.h"
#ifdef IOTSA_HAS_WEBSERVER
#include "iotsaHttpServer.h"
#endif

//
// One API request, the same over REST, HPS and CoAP (cwi-dis/iotsa#280,
// cwi-dis/iotsa#296). All handlers run in the loop task, one request at a time
// (#236).
//

static String _joined(const std::vector<String>& names) {
  String rv;
  for (const String& n : names) {
    if (rv.length()) rv += ", ";
    rv += n;
  }
  return rv;
}

static const char *_methodName(IotsaApiOperation verb) {
  return verb == IOTSA_API_GET ? "GET" : verb == IOTSA_API_PUT ? "PUT" : "POST";
}

void iotsaApiLogRequest(const char *transport, const char *method, const char *path, const String& detail) {
  IOTSA_LOG_DEBUG("iotsaApi", "%s: %s %s%s%s -> ...", transport, method, path, detail.length() ? " " : "", detail.c_str());
}

void iotsaApiLogResult(const char *transport, const char *method, const char *path, const String& result) {
  IOTSA_LOG("iotsaApi", "%s: %s %s -> %s", transport, method, path, result.c_str());
}

static int _errorReply(const char *transport, const char *method, const char *path, int status, const String& message, String& reply) {
  JsonDocument errorDocument;
  errorDocument["iotsa_api_error"] = message;
  reply = "";
  serializeJson(errorDocument, reply);
  iotsaApiLogResult(transport, method, path, String(status) + " " + message);
  return status;
}

int iotsaApiRefusal(const char *transport, const char *method, const char *path, int status, const char *message, String& reply) {
  iotsaApiLogRequest(transport, method, path, "");
  return _errorReply(transport, method, path, status, message, reply);
}

// The defaults, for a verb the module registered but doesn't handle.
void IotsaApiProvider::getHandler(IotsaApiContext& ctx) {
  ctx.error(405, "method not allowed");
}

void IotsaApiProvider::putHandler(IotsaApiContext& ctx) {
  ctx.error(405, "method not allowed");
}

void IotsaApiProvider::postHandler(IotsaApiContext& ctx) {
  ctx.error(405, "method not allowed");
}

int iotsaApiRequest(IotsaApiProvider *provider, const char *transport, IotsaApiOperation verb, const char *path, const char *body, bool http, String& reply, bool& responded) {
  const char *method = _methodName(verb);
  // The size of the body only: it may hold a password. The body itself only
  // with IOTSA_DEBUG_API.
  iotsaApiLogRequest(transport, method, path, verb == IOTSA_API_GET ? String() : "(" + String((unsigned)(body ? strlen(body) : 0)) + " bytes)");
  if (verb != IOTSA_API_GET) IOTSA_LOG_DEBUG_API("iotsaApi", "%s: request %s", transport, body ? body : "");
  responded = false;
  IotsaBaseModule *mod = provider->asModule();
  if (mod == nullptr) return _errorReply(transport, method, path, 500, "no module", reply);
  JsonDocument requestDocument;
  JsonVariant request;
  if (verb != IOTSA_API_GET) {
    DeserializationError err = deserializeJson(requestDocument, body ? body : "");
    if (err == DeserializationError::NoMemory || requestDocument.overflowed()) {
      return _errorReply(transport, method, path, 413, "request too big", reply);
    } else if (err && err != DeserializationError::EmptyInput) {
      return _errorReply(transport, method, path, 400, "invalid JSON", reply);
    }
    request = requestDocument.as<JsonVariant>();
  }
  JsonDocument replyDocument;
  JsonObject replyObject = replyDocument.to<JsonObject>();
#ifdef IOTSA_HAS_WEBSERVER
  IotsaWebServer *server = http ? IotsaHttpServiceMod::instance()->server : nullptr;
  IotsaApiContext ctx(*mod, transport, verb, path, request, replyObject, server);
#else
  IotsaApiContext ctx(*mod, transport, verb, path, request, replyObject);
#endif
  if (verb == IOTSA_API_GET) {
    provider->getHandler(ctx);
  } else if (verb == IOTSA_API_PUT) {
    provider->putHandler(ctx);
  } else {
    provider->postHandler(ctx);
  }
  // Fields read before an error were changed, so a Saved one is saved anyway.
  if (ctx.configChanged) {
    mod->configSave();
    iotsaController.extendCurrentMode();
    iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_MAGENTA, 0, 0, 2000, "Settings saved");  // cwi-dis/iotsa#176
  }
  if (ctx.responded()) {
    // A 401 challenge, already sent.
    responded = true;
    iotsaApiLogResult(transport, method, path, String(ctx.errorStatus()) + " " + ctx.errorMessage());
    return ctx.errorStatus();
  }
  if (ctx.failed()) return _errorReply(transport, method, path, ctx.errorStatus(), ctx.errorMessage(), reply);
  ctx.reportUnused();
  if (replyDocument.overflowed()) return _errorReply(transport, method, path, 413, "reply too big", reply);
  if (!ctx.ignored().empty()) {
    JsonArray ignored = replyDocument["iotsa_api_ignored"].to<JsonArray>();
    for (const String& n : ctx.ignored()) ignored.add(n);
  }
  if (!ctx.redacted().empty()) {
    JsonArray redacted = replyDocument["iotsa_api_redacted"].to<JsonArray>();
    for (const String& n : ctx.redacted()) redacted.add(n);
  }
  reply = "";
  serializeJson(replyDocument, reply);
  String result = "ok";
  if (ctx.configChanged) result += ", saved";
  if (!ctx.ignored().empty()) result += ", ignored " + _joined(ctx.ignored());
  iotsaApiLogResult(transport, method, path, result);
  IOTSA_LOG_DEBUG_API("iotsaApi", "%s: reply %s", transport, reply.c_str());
  return 200;
}
