#include "iotsa.h"
#include "iotsaApi.h"

//
// The outcome of the API request being handled, shared by the REST, HPS and
// CoAP transports (cwi-dis/iotsa#280). See IotsaApiResult in iotsa.h.
//
IotsaApiResult iotsaApiResult;

bool iotsaApiError(int status, const char *message) {
  // The first error wins: it is usually the most specific one.
  if (iotsaApiResult.status == 0) {
    iotsaApiResult.status = status;
    iotsaApiResult.message = message;
  }
  return false;
}

void _iotsaApiNoteType(const char *name, bool rightType) {
  std::vector<String>& wt = iotsaApiResult.wrongType;
  for (auto it = wt.begin(); it != wt.end(); ++it) {
    if (*it == name) {
      if (rightType) wt.erase(it);
      return;
    }
  }
  if (!rightType) wt.push_back(name);
}

void iotsaApiBegin(const char *transport, const char *method, const char *path) {
  iotsaApiResult = IotsaApiResult();
  iotsaApiResult.transport = transport;
  iotsaApiResult.method = method;
  iotsaApiResult.path = path;
  IOTSA_LOG("iotsaApi", "%s: %s %s", transport, method, path);
}

static String _joined(const std::vector<String>& names) {
  String rv;
  for (const String& n : names) {
    if (rv.length()) rv += ", ";
    rv += n;
  }
  return rv;
}

int iotsaApiEnd(bool ok, JsonDocument& replyDocument, String& body) {
  IotsaApiResult& r = iotsaApiResult;
  int status = 200;
  String message;
  if (r.authResponded) {
    status = r.status ? r.status : 401;
    message = r.message.length() ? r.message : "not authorized";
  } else if (!r.wrongType.empty()) {
    status = 400;
    message = "wrong type: " + _joined(r.wrongType);
  } else if (!ok) {
    status = r.status ? r.status : 400;
    message = r.message.length() ? r.message : "bad request";
  } else if (replyDocument.overflowed()) {
    status = 413;
    message = "reply too big";
  }
  body = "";
  if (status == 200) {
    if (!r.ignored.empty()) {
      JsonArray ignored = replyDocument["iotsa_api_ignored"].to<JsonArray>();
      for (const String& n : r.ignored) ignored.add(n);
    }
    serializeJson(replyDocument, body);
    if (r.ignored.empty()) {
      IOTSA_LOG("iotsaApi", "%s: %s %s -> ok", r.transport, r.method, r.path.c_str());
    } else {
      IOTSA_LOG("iotsaApi", "%s: %s %s -> ok, ignored %s", r.transport, r.method, r.path.c_str(), _joined(r.ignored).c_str());
    }
  } else {
    JsonDocument errorDocument;
    errorDocument["iotsa_api_error"] = message;
    serializeJson(errorDocument, body);
    IOTSA_LOG("iotsaApi", "%s: %s %s -> %d %s", r.transport, r.method, r.path.c_str(), status, message.c_str());
  }
  return status;
}
