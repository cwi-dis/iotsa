#ifndef _IOTSACONTEXT_H_
#define _IOTSACONTEXT_H_
#include "iotsa.h"

//
// One per request, for every transport: the shared base of IotsaWebContext
// (web pages, triggers) and the API context (cwi-dis/iotsa#296). It holds what
// doesn't depend on how the request arrived: the rights, the persistence of a
// change and the access check, the first error, and which request fields the
// module used.
//
class IotsaContext {
  friend class IotsaBaseModule;   // raw web handlers
  friend class IotsaModule;       // triggers
public:
  IotsaContext(IotsaBaseModule& mod, const String& apiPath);
  virtual ~IotsaContext() {}

  // ---- for auth providers ----

  // The credentials the request carries. None, unless the transport has them.
  virtual bool hasBasicAuth(const char *user, const char *password) { return false; }
  virtual String bearerToken() { return ""; }
  // Whether the transport can carry credentials at all (HTTP can, HPS and
  // CoAP can't yet, cwi-dis/iotsa#303).
  virtual bool carriesCredentials() const { return false; }
  // This verb on this API path: what a capability is checked against.
  const String& apiPath() const { return _apiPath; }
  IotsaApiOperation verb() const { return _verb; }
  void setVerb(IotsaApiOperation verb) { _verb = verb; }

  // What a change does to the module's saved state. Every arg*() and show*()
  // call says it. Ordered: a later one is stricter.
  enum Persistence {
    Transient,        // changes state, nothing saved (alarm, LED, isOn)
    Saved,            // the framework calls the module's configSave() afterwards
    SavedInConfigMode // the same, and only in configuration mode or on a factory-fresh
                      // device's own AP, iotsaConfigSettingsWritable() (hostName, ssid)
  };

  // Changes after this need this named right (as in IotsaUserMod). The
  // default is the module's (changesNeedRight()), which is IOTSA_RIGHT_NONE
  // unless the module says otherwise: a change that names no right is open to
  // everyone on the network (https://github.com/cwi-dis/iotsa/issues/183#issuecomment-6368033789).
  // Checked lazily, at the first change, so just reading needs no right. Can
  // change halfway through the put side: the calls after it use the new right.
  void argsNeedRight(const char *right) { _setRight(right); }
  // Reading the fields after this needs this right. The default is the
  // module's (readsNeedRight()), IOTSA_RIGHT_NONE unless it says otherwise.
  // IOTSA_RIGHT_NEVER: a write-only value, such as a password.
  void showsNeedRight(const char *right) { _readRight = right; }
  // Back to the module's rights. The framework does this between the put
  // side and each part of the get side, so formHandler_fields() declares its
  // rights itself, the same ones as formHandler_args().
  void resetRights();

  // The first error stops the rest of the request: later arg*() calls do
  // nothing and return false, a later error() is ignored, and the reply
  // carries this one. Side effects of a put side come after
  // "if (ctx.failed()) return;".
  void error(int status, const String& message);
  bool failed() const { return _status != 0; }
  int errorStatus() const { return _status; }
  const String& errorMessage() const { return _message; }

  // Filled in by the put side, read by the framework.
  bool configChanged = false;     // saved, mode window extended, LED pulse
  bool stateChanged = false;      // nothing to save

  // A change to the device's long-term state that the module may still refuse
  // (add, remove, upload), triggered by its own button. Its fields are read
  // with the usual calls, and each still checks its own access; one refused
  // field fails the whole Change. Only marking waits: done() marks the
  // strictest persistence seen (at least the Change's own), once the change
  // really happened.
  class Change {
  public:
    Change(IotsaContext& ctx, const char *button, Persistence persistence);
    ~Change();
    // The button is present, the right and mode allow it, and none of its
    // fields was refused.
    explicit operator bool() const { return _active && !_ctx.failed(); }
    void done();
  private:
    IotsaContext& _ctx;
    Persistence _persistence;
    bool _active = false;
    bool _outer;                    // inside another Change: that one decides
  };

protected:
  // The declared rights, saved and restored around an object, so the
  // object's argsNeedRight()/showsNeedRight() don't leak into its module.
  struct _Rights {
    const char *right;
    const char *readRight;
    bool authChecked;
    IotsaAccess authAnswer;
  };
  _Rights _saveRights() const { return {_right, _readRight, _authChecked, _authAnswer}; }
  void _restoreRights(const _Rights& r) { _right = r.right; _readRight = r.readRight; _authChecked = r.authChecked; _authAnswer = r.authAnswer; }

public:
  // While it lives, the rights declared (argsNeedRight(), showsNeedRight())
  // are its own: they end with it. For a part of a handler that needs a
  // right the rest doesn't (renaming a device, next to the device's own
  // fields).
  class RightsScope {
  public:
    RightsScope(IotsaContext& ctx) : _ctx(ctx), _saved(ctx._saveRights()) {}
    ~RightsScope() { _ctx._restoreRights(_saved); }
  private:
    IotsaContext& _ctx;
    _Rights _saved;
  };

protected:
  // Is the (scoped) argument in the request? Marks it as used.
  virtual bool _hasArg(const char *name) { return false; }
  // Ask for credentials (a 401 with WWW-Authenticate), refuse (a 403): only a
  // transport that answers by itself does these.
  virtual void _challenge() {}
  virtual void _forbid(const String& message) {}
  // The challenge was sent: the request is answered.
  virtual void _authRefused() {}
  // For messages: "rest", "hps", ...
  virtual const char *_transport() const { return "this transport"; }

  // A right, asked of the auth provider. nullptr: no right needed, as
  // IOTSA_RIGHT_NONE.
  IotsaAccess _access(const char *right);
  // For raw web handlers: refused means the reply was sent.
  bool _refuse(const char *right);
  // The write right for the calls now: asked once per declared right.
  IotsaAccess _writeAccess();
  // The access check for a change of this persistence: right, then mode.
  bool _mayChange(Persistence persistence);
  // Whether a field of this persistence can be changed in the current mode.
  bool _modeAllows(Persistence persistence) const;
  void _changed(Persistence persistence);
  void _mark(Persistence persistence);
  // Compare old and new value before the access check? Only for fields
  // everybody may read: otherwise "unchanged, so not refused" would tell a
  // right guess of a protected value from a wrong one.
  bool _compareFirst() const;
  // May the caller read the fields after showsNeedRight()? empty: the value
  // is empty, which a write-only (IOTSA_RIGHT_NEVER) value may show.
  bool _mayRead(bool empty);
  // A read was refused because the request had no credentials: logging in
  // would help. A web page then asks for them.
  bool _readNeedsCredentials = false;

  void _use(const String& name);
  bool _isUsed(const String& name) const;

  IotsaBaseModule& _mod;
  String _apiPath;

private:
  void _setRight(const char *right);

  const char *_right = IOTSA_RIGHT_NONE;
  const char *_readRight = IOTSA_RIGHT_NONE;
  IotsaApiOperation _verb = IOTSA_API_PUT;
  bool _authChecked = false;
  IotsaAccess _authAnswer = IotsaAccess::Granted;
  bool _inChange = false;           // reading a Change's fields
  Persistence _seen = Transient;    // strictest change seen inside the Change
  bool _seenAny = false;
  int _status = 0;
  String _message;
  std::vector<String> _used;
};

#ifdef IOTSA_HAS_WEBSERVER
// A request that arrived over HTTP(S): the credentials are in its headers.
class IotsaHttpContext : public IotsaContext {
public:
  IotsaHttpContext(IotsaBaseModule& mod, IotsaWebServer *server, const String& apiPath)
  : IotsaContext(mod, apiPath), _server(server) {}
  bool hasBasicAuth(const char *user, const char *password) override;
  String bearerToken() override;
  bool carriesCredentials() const override { return _server != nullptr; }
protected:
  void _challenge() override;
  void _forbid(const String& message) override;
  IotsaWebServer *_server;
};
typedef IotsaHttpContext IotsaApiContextBase;
#else
typedef IotsaContext IotsaApiContextBase;
#endif

//
// One per API request (REST, CoAP, HPS), for getHandler(), putHandler() and
// postHandler() (cwi-dis/iotsa#296). arg*() reads a field from the JSON
// request, with the same access check and persistence as on a web page;
// show() writes a field to the reply, if the caller may read it, otherwise
// the field is listed in "iotsa_api_redacted".
//
class IotsaApiContext : public IotsaApiContextBase {
public:
#ifdef IOTSA_HAS_WEBSERVER
  // server: the HTTP server the request came from (REST), for its
  // credentials; nullptr for transports without credentials.
  IotsaApiContext(IotsaBaseModule& mod, const char *transport, IotsaApiOperation verb, const String& path, JsonVariant request, JsonObject reply, IotsaWebServer *server = nullptr);
#else
  IotsaApiContext(IotsaBaseModule& mod, const char *transport, IotsaApiOperation verb, const String& path, JsonVariant request, JsonObject reply);
#endif

  const String& path() const { return _apiPath; }
  // The JSON request and reply, for what arg*() and show() can't express
  // (nested objects). Mark the request fields you used with used().
  // requestVariant(): a request that isn't an object (an array).
  JsonObject request() { return _request; }
  JsonVariant requestVariant() { return _requestVariant; }
  JsonObject reply() { return _reply; }
  void used(const char *name) { _use(_prefix + name); }

  // An object inside the module (a button, a user): while a Scope lives,
  // arg*() reads from request and show() writes to reply, the object's own
  // parts. Its unused and redacted fields are reported as "<prefix>.<name>".
  // The rights it declares end with the Scope.
  class Scope {
  public:
    Scope(IotsaApiContext& ctx, const String& prefix, JsonObject request, JsonObject reply);
    ~Scope();
  private:
    IotsaApiContext& _ctx;
    String _savedPrefix;
    JsonObject _savedRequest;
    JsonObject _savedReply;
    _Rights _savedRights;
  };

  // ---- put side ----

  // If the field is present and differs from field: check access and mode,
  // assign. Returns true if field changed. A field of the wrong type is an
  // error (400). A bool also accepts 0 and 1.
  template<typename T> bool arg(const char *name, T& field, Persistence persistence) {
    JsonVariant v;
    if (!_present(name, v)) return false;
    T newValue;
    if (!_fromJson(v, newValue)) {
      error(400, String("wrong type: ") + name);
      return false;
    }
    bool same = newValue == field;
    if (same && _compareFirst()) return false;
    if (!_mayChange(persistence)) return false;
    if (same) return false;
    field = newValue;
    _changed(persistence);
    return true;
  }
  // A secret: always checked before comparing. An empty string empties it.
  bool argPassword(const char *name, String& field, Persistence persistence);
  // The field is present and the right and mode allow it, whatever its
  // value: a command that is a change by itself. Read the value with
  // ctx.request()[name].
  bool argAction(const char *name, Persistence persistence);

  // ---- get side ----

  // Write name: value to the reply, or redact it.
  template<typename T> void show(const char *name, const T& value) {
    if (_mayRead(_isEmpty(value))) {
      _reply[name] = value;
    } else {
      _redact(name);
    }
  }
  // A secret that isn't in a variable (a file): "" when not set, otherwise
  // redacted. As show() under IOTSA_RIGHT_NEVER.
  void showWriteOnly(const char *name, bool isSet);

  // For iotsaApiRequest(), after the handler: the request fields nothing
  // used, the fields the caller may not read, and whether the reply was
  // already sent (a 401 challenge).
  void reportUnused();
  const std::vector<String>& ignored() const { return _ignored; }
  const std::vector<String>& redacted() const { return _redacted; }
  bool responded() const { return _responded; }

protected:
  bool _hasArg(const char *name) override { JsonVariant v; return _present(name, v); }
  void _authRefused() override;
  const char *_transport() const override { return _transportName; }

private:
  bool _present(const char *name, JsonVariant& value);
  void _redact(const char *name);
  static bool _fromJson(JsonVariant v, bool& out);
  static bool _fromJson(JsonVariant v, String& out);
  template<typename T> static bool _fromJson(JsonVariant v, T& out) {
    if (!v.is<T>()) return false;
    out = v.as<T>();
    return true;
  }
  static bool _isEmpty(const String& v) { return v.length() == 0; }
  static bool _isEmpty(const char *v) { return v == nullptr || *v == '\0'; }
  template<typename T> static bool _isEmpty(const T& v) { return false; }

  void _reportUnused(JsonObject request);

  const char *_transportName;
  JsonVariant _requestVariant;
  JsonObject _request;
  JsonObject _reply;
  String _prefix;
  std::vector<String> _ignored;
  std::vector<String> _redacted;
  bool _responded = false;
};

#endif
