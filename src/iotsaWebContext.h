#ifndef _IOTSAWEBCONTEXT_H_
#define _IOTSAWEBCONTEXT_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
//
// One per web request, shared by the put side (formHandler_args) and the get
// side (formHandler_fields) of a module and its objects. IotsaModule's default
// webHandler() creates it, calls both halves, and builds the page around them:
// head, title, feedback lines, the two sections, saving the configuration.
// See cwi-dis/iotsa#183 (put side) and cwi-dis/iotsa#292 (get side).
//

// A label for a field or status line: a plain string, or F("...") so the text
// stays in flash on the ESP8266.
class IotsaWebLabel {
public:
  IotsaWebLabel(const char *s) : text(s) {}
  IotsaWebLabel(const String& s) : text(s) {}
  IotsaWebLabel(const __FlashStringHelper *s) : text(s) {}
  String text;
};

class IotsaWebContext {
public:
  IotsaWebContext(IotsaBaseModule& mod, IotsaWebServer *server, const String& apiPath);

  // ---- put side ----

  enum FieldKind {
    Live,           // state, not saved (alarm, LED, isOn)
    Config,         // saved
    ConfigModeOnly  // saved, only writable in configuration mode (hostName, ssid)
  };

  // If the argument is present and differs from field: check auth (once per
  // request), check configuration mode, parse, assign. Returns true if field
  // changed. A refused or unparseable change is reported on the page instead.
  template<typename T> bool arg(const char *name, T& field, FieldKind kind = Config) {
    String value;
    if (!_present(name, value)) return false;
    T newValue;
    if (!_parse(value, newValue)) {
      error(String("Invalid value for ") + _scoped(name) + ".");
      return false;
    }
    if (newValue == field) return false;
    if (!_mayChange(kind)) return false;
    field = newValue;
    _changed(kind);
    return true;
  }
  // A checkbox (written by checkbox() on the get side, with its hidden 0).
  bool checkbox(const char *name, bool& field, FieldKind kind = Config) { return arg(name, field, kind); }
  // A password: an empty one means "leave unchanged".
  bool password(const char *name, String& field, FieldKind kind = Config);
  // A button without fields (identify, reboot, remove): present and allowed.
  bool action(const char *name);

  // Changes on this page need this named right (as in IotsaMultiUserMod),
  // instead of the REST rule (PUT on the module's API path). Checked lazily, at
  // the first change, so just viewing the page needs no right. Both kinds of
  // check exist until cwi-dis/iotsa#284 decides between them.
  void needsRight(const char *right) { _right = right; }

  // Lines shown at the top of the page.
  void feedback(const String& text);
  void error(const String& text);
  // Answer the request yourself (plain text, 404): the framework sends nothing more.
  void respond(int code, const char *type, const String& body);

  // Filled in by the calls above, read by the framework.
  bool configChanged = false;     // saved, mode window extended, LED pulse
  bool stateChanged = false;      // only "Done." on the page
  bool refusedConfigMode = false; // "must be in configuration mode"
  bool responded = false;         // auth failed (challenge sent), or respond() was called

  // ---- objects inside a module ----

  // While a Scope lives, names get "<prefix>." in front, so an object writes
  // ctx.arg("url", url) and the page sends "button1.url".
  class Scope {
  public:
    Scope(IotsaWebContext& ctx, const String& prefix);
    ~Scope();
  private:
    IotsaWebContext& _ctx;
    String _saved;
  };

  // ---- get side ----

  enum Section { Everyday, Configuration };
  Section section = Everyday;
  // Whether a field of this kind can be edited now.
  bool editable(FieldKind kind) const;

  // A group of fields that wants to be a form, with a submit button. Written as
  // a <form> unless a form is already open, then as a <fieldset> inside it. No
  // form and no button when nothing in it is editable. POST when it contains a
  // password (cwi-dis/iotsa#183), otherwise GET.
  class Form {
  public:
    Form(IotsaWebContext& ctx, const IotsaWebLabel& submitLabel = "Save");
    ~Form();
  private:
    IotsaWebContext& _ctx;
  };

  // Read-only line. name is the REST field it comes from (written as the id),
  // or "" when it has no REST counterpart.
  void status(const IotsaWebLabel& label, const char *name, const String& value);
  // An input when editable, otherwise the same as status().
  void field(const IotsaWebLabel& label, const char *name, const String& value, FieldKind kind = Config, const char *hint = nullptr);
  void field(const IotsaWebLabel& label, const char *name, const char *value, FieldKind kind = Config, const char *hint = nullptr) { field(label, name, String(value), kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, int value, FieldKind kind = Config, const char *hint = nullptr) { _field(label, name, String(value), "number", kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, unsigned int value, FieldKind kind = Config, const char *hint = nullptr) { _field(label, name, String(value), "number", kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, long value, FieldKind kind = Config, const char *hint = nullptr) { _field(label, name, String(value), "number", kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, unsigned long value, FieldKind kind = Config, const char *hint = nullptr) { _field(label, name, String(value), "number", kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, float value, FieldKind kind = Config, const char *hint = nullptr) { _field(label, name, String(value), "number' step='any", kind, hint); }
  void field(const IotsaWebLabel& label, const char *name, bool value, FieldKind kind = Config, const char *hint = nullptr) { checkbox(label, name, value, kind, hint); }
  void checkbox(const IotsaWebLabel& label, const char *name, bool value, FieldKind kind = Config, const char *hint = nullptr);
  // Never prefilled.
  void password(const IotsaWebLabel& label, const char *name, FieldKind kind = Config, const char *hint = nullptr);
  // Raw HTML, not escaped.
  IotsaWebContext& operator<<(const String& html);

  // Used by IotsaModule::webHandler().
  void sendPage(const String& title, const String& everyday, const String& configuration);
  String takeOutput();

private:
  struct Group {
    String html;
    String submitLabel;
    bool editable = false;
    bool post = false;
  };

  bool _present(const char *name, String& value);
  bool _mayChange(FieldKind kind);
  void _changed(FieldKind kind);
  void _field(const IotsaWebLabel& label, const char *name, const String& value, const char *type, FieldKind kind, const char *hint);
  String _scoped(const char *name) const;
  String& _out();
  void _markEditable(bool post);
  void _closeForm();

  static bool _parse(const String& s, String& v) { v = s; return true; }
  static bool _parse(const String& s, bool& v);
  static bool _parse(const String& s, int& v);
  static bool _parse(const String& s, unsigned int& v);
  static bool _parse(const String& s, long& v);
  static bool _parse(const String& s, unsigned long& v);
  static bool _parse(const String& s, float& v);

  IotsaBaseModule& _mod;
  IotsaWebServer *_server;
  String _apiPath;
  const char *_right = nullptr;
  bool _authChecked = false;
  bool _authOk = false;
  String _prefix;
  String _output;
  std::vector<Group> _forms;
  std::vector<String> _feedback;
};

#endif // IOTSA_WITH_WEB
#endif
