#include "iotsaContext.h"

IotsaContext::IotsaContext(IotsaBaseModule& mod, const String& apiPath)
: _mod(mod),
  _apiPath(apiPath)
{
  resetRights();
}

void IotsaContext::resetRights() {
  _right = _mod.changeRight();
  _readRight = _mod.readRight();
  _authChecked = false;
}

void IotsaContext::error(int status, const String& message) {
  // The first error wins: it stopped the request, the rest is a consequence.
  if (_status) return;
  _status = status;
  _message = message;
}

IotsaAccess IotsaContext::_access(const char *right) {
  if (right == nullptr || strcmp(right, IOTSA_RIGHT_NONE) == 0) return IotsaAccess::Granted;
  if (strcmp(right, IOTSA_RIGHT_NEVER) == 0) return IotsaAccess::Forbidden;
  return _mod.app.getAuth()->allows(*this, right);
}

bool IotsaContext::_refuse(const char *right) {
  IotsaAccess access = _access(right);
  if (access == IotsaAccess::Granted) return false;
  if (access == IotsaAccess::NeedsCredentials) {
    _challenge();
  } else {
    _forbid(right ? String("Not allowed: needs the right \"") + right + "\"." : String("Not allowed."));
  }
  return true;
}

IotsaAccess IotsaContext::_writeAccess() {
  if (!_authChecked) {
    _authChecked = true;
    _authAnswer = _access(_right);
  }
  return _authAnswer;
}

bool IotsaContext::_mayChange(Persistence persistence) {
  if (failed()) return false;
  const char *right = _right;
  _writeAccess();
  if (_authAnswer == IotsaAccess::NeedsCredentials) {
    if (!carriesCredentials()) {
      // A 401 once the transport can carry them (cwi-dis/iotsa#303).
      error(501, String("credentials not supported over ") + _transport());
      return false;
    }
    error(401, "Not authorized.");
    _challenge();
    _authRefused();
    return false;
  }
  if (_authAnswer == IotsaAccess::Forbidden) {
    if (right && strcmp(right, IOTSA_RIGHT_NEVER) == 0) {
      error(403, "This can't be changed.");
    } else if (right) {
      error(403, String("Not allowed: needs the right \"") + right + "\".");
    } else {
      error(403, "Not allowed.");
    }
    return false;
  }
  if (!_modeAllows(persistence)) {
    error(409, "Some settings can only be changed in configuration mode.");
    return false;
  }
  return true;
}

bool IotsaContext::_modeAllows(Persistence persistence) const {
  if (persistence == SavedInConfigMode) return iotsaConfigSettingsWritable();
  return true;
}

void IotsaContext::_setRight(const char *right) {
  bool sameRight = right == _right || (right && _right && strcmp(right, _right) == 0);
  if (sameRight) return;
  _right = right;
  _authChecked = false;
}

bool IotsaContext::_compareFirst() const {
  return _readRight == nullptr || strcmp(_readRight, IOTSA_RIGHT_NONE) == 0;
}

bool IotsaContext::_mayRead(bool empty) {
  if (_readRight == nullptr || strcmp(_readRight, IOTSA_RIGHT_NONE) == 0) return true;
  // An empty secret gives nothing away.
  if (strcmp(_readRight, IOTSA_RIGHT_NEVER) == 0) return empty;
  IotsaAccess access = _access(_readRight);
  if (access == IotsaAccess::NeedsCredentials) _readNeedsCredentials = true;
  return access == IotsaAccess::Granted;
}

void IotsaContext::_changed(Persistence persistence) {
  // Inside a Change only the strictest persistence is remembered: done() marks.
  if (_inChange) {
    if (!_seenAny || persistence > _seen) _seen = persistence;
    _seenAny = true;
    return;
  }
  _mark(persistence);
}

void IotsaContext::_mark(Persistence persistence) {
  if (persistence == Transient) {
    stateChanged = true;
  } else {
    configChanged = true;
  }
}

void IotsaContext::_use(const String& name) {
  if (!_isUsed(name)) _used.push_back(name);
}

bool IotsaContext::_isUsed(const String& name) const {
  for (const String& n : _used) {
    if (n == name) return true;
  }
  return false;
}

IotsaContext::Change::Change(IotsaContext& ctx, const char *button, Persistence persistence)
: _ctx(ctx),
  _persistence(persistence),
  _outer(ctx._inChange)
{
  if (!_ctx._hasArg(button)) return;
  if (!_ctx._mayChange(persistence)) return;
  _active = true;
  if (!_outer) {
    _ctx._seen = persistence;
    _ctx._seenAny = true;
  }
  _ctx._inChange = true;
}

IotsaContext::Change::~Change() {
  _ctx._inChange = _outer;
  if (!_outer) _ctx._seenAny = false;
}

void IotsaContext::Change::done() {
  if (!_active || _outer || _ctx.failed()) return;
  _ctx._mark(_ctx._seen > _persistence ? _ctx._seen : _persistence);
}

#ifdef IOTSA_HAS_WEBSERVER
bool IotsaHttpContext::hasBasicAuth(const char *user, const char *password) {
  if (_server == nullptr) return false;
  return _server->authenticate(user, password);
}

String IotsaHttpContext::bearerToken() {
  if (_server == nullptr || !_server->hasHeader("Authorization")) return "";
  String header = _server->header("Authorization");
  if (!header.startsWith("Bearer ")) return "";
  return header.substring(7);
}

void IotsaHttpContext::_challenge() {
  if (_server) _server->requestAuthentication();
  _sentStatus = 401;
  _sentMessage = "credentials needed";
}

void IotsaHttpContext::_forbid(const String& message) {
  if (_server) _server->send(403, "text/plain", message + "\n");
  _sentStatus = 403;
  _sentMessage = message;
}
#endif

//
// IotsaApiContext
//

#ifdef IOTSA_HAS_WEBSERVER
IotsaApiContext::IotsaApiContext(IotsaBaseModule& mod, const char *transport, IotsaApiOperation verb, const String& path, JsonVariant request, JsonObject reply, IotsaWebServer *server)
: IotsaHttpContext(mod, server, path),
#else
IotsaApiContext::IotsaApiContext(IotsaBaseModule& mod, const char *transport, IotsaApiOperation verb, const String& path, JsonVariant request, JsonObject reply)
: IotsaContext(mod, path),
#endif
  _transportName(transport),
  _requestVariant(request),
  _request(request.as<JsonObject>()),
  _reply(reply)
{
  setVerb(verb);
}

IotsaApiContext::Scope::Scope(IotsaApiContext& ctx, const String& prefix, JsonObject request, JsonObject reply)
: _ctx(ctx),
  _savedPrefix(ctx._prefix),
  _savedRequest(ctx._request),
  _savedReply(ctx._reply),
  _savedRights(ctx._saveRights())
{
  _ctx._prefix = _savedPrefix + prefix + ".";
  _ctx._request = request;
  _ctx._reply = reply;
}

IotsaApiContext::Scope::~Scope() {
  _ctx._reportUnused(_ctx._request);
  _ctx._prefix = _savedPrefix;
  _ctx._request = _savedRequest;
  _ctx._reply = _savedReply;
  _ctx._restoreRights(_savedRights);
}

bool IotsaApiContext::_present(const char *name, JsonVariant& value) {
  if (failed() || _request.isNull()) return false;
  JsonVariant v = _request[name];
  if (v.isNull()) return false;
  _use(_prefix + name);
  value = v;
  return true;
}

bool IotsaApiContext::_fromJson(JsonVariant v, bool& out) {
  // Both true/false and 1/0 (cwi-dis/iotsa#261).
  if (v.is<bool>()) {
    out = v.as<bool>();
    return true;
  }
  if (v.is<long>()) {
    out = v.as<long>() != 0;
    return true;
  }
  return false;
}

bool IotsaApiContext::_fromJson(JsonVariant v, String& out) {
  if (!v.is<const char *>()) return false;
  out = v.as<const char *>();
  return true;
}

bool IotsaApiContext::argPassword(const char *name, String& field, Persistence persistence) {
  JsonVariant v;
  if (!_present(name, v)) return false;
  String value;
  if (!_fromJson(v, value)) {
    error(400, String("wrong type: ") + name);
    return false;
  }
  // Check before comparing: a secret is never readable, so "unchanged, not
  // refused" must not tell a right guess from a wrong one.
  if (!_mayChange(persistence)) return false;
  if (value == field) return false;
  field = value;
  _changed(persistence);
  return true;
}

bool IotsaApiContext::argAction(const char *name, Persistence persistence) {
  JsonVariant v;
  if (!_present(name, v)) return false;
  if (!_mayChange(persistence)) return false;
  _changed(persistence);
  return true;
}

void IotsaApiContext::showWriteOnly(const char *name, bool isSet) {
  if (isSet) {
    _redact(name);
  } else {
    _reply[name] = "";
  }
}

void IotsaApiContext::_redact(const char *name) {
  _redacted.push_back(_prefix + name);
}

void IotsaApiContext::reportUnused() {
  _reportUnused(_request);
}

void IotsaApiContext::_reportUnused(JsonObject request) {
  // After an error later fields weren't read: that is not "ignored".
  if (failed() || request.isNull()) return;
  for (JsonPair kv : request) {
    String name = _prefix + kv.key().c_str();
    if (!_isUsed(name)) _ignored.push_back(name);
  }
}

void IotsaApiContext::_authRefused() {
  // The challenge was sent: the transport sends nothing more.
  _responded = true;
}
