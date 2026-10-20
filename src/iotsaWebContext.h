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
    ConfigModeOnly, // saved, only writable in configuration mode or on a factory-fresh
                    // device's own AP, iotsaConfigSettingsWritable() (hostName, ssid)
    ConfigModeStrict // saved, only writable in configuration mode (trust anchors)
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
  // A password: an empty one means "leave unchanged", unless allowEmpty (an
  // open network, but only together with a new network name). clearable: an
  // optional secret, its "clear" checkbox empties it.
  bool password(const char *name, String& field, FieldKind kind = Config, bool allowEmpty = false, bool clearable = false);
  // One of count choices, 0..count-1 (written by choice() on the get side).
  bool choice(const char *name, int& field, int count, FieldKind kind = Config);
  // A button without fields (identify, reboot, remove): present and allowed.
  // Counts as a change of the given kind.
  bool action(const char *name, FieldKind kind = Live);
  // Read an argument without any check or effect (hidden bookkeeping fields,
  // a collection's version).
  bool peek(const char *name, String& value) { return _present(name, value); }
  // A button that needs its "I am sure" box ticked (factory reset, remove all).
  bool confirmedAction(const char *name);
  // The right and the mode allow a change of this kind, without an argument
  // that asks for it: a trigger, which is a change by itself.
  bool allowed(FieldKind kind = Live) { return _mayChange(kind); }

  // A change to the device's long-term state that the module may still refuse
  // (add, remove, upload): triggered by its own button. While it lives, its
  // parameters are read with the usual calls, but they only read: no mode
  // check of their own and no change marked. done() marks the change, once it
  // really happened. action() is for things that just happen now.
  class Change {
  public:
    Change(IotsaWebContext& ctx, const char *button, FieldKind kind = Config);
    ~Change();
    // The button is present, and the right and mode allow it.
    explicit operator bool() const { return _active; }
    void done();
  private:
    IotsaWebContext& _ctx;
    FieldKind _kind;
    bool _active = false;
    bool _outer;  // inside another Change: that one decides
  };

  // Changes on this page need this named right (as in IotsaUserMod),
  // instead of the REST rule (PUT on the module's API path). Checked lazily, at
  // the first change, so just viewing the page needs no right. Both kinds of
  // check exist until cwi-dis/iotsa#284 decides between them. Can change
  // halfway through formHandler_args(): the calls after it use the new right.
  // IOTSA_RIGHT_NONE: the changes after it need no right at all.
  void needsRight(const char *right) { _setRule(NamedRight, right); }

  // Lines shown at the top of the page.
  void feedback(const String& text);
  void error(const String& text);
  bool hasFeedback() const { return !_feedback.empty(); }
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
  // password (cwi-dis/iotsa#183) or a button (an action is not idempotent, a
  // reload shouldn't repeat it), otherwise GET. The submit button is only
  // there when the group has fields. The heading becomes an <h3> (or the
  // fieldset's legend).
  class Form {
  public:
    // submitName (scoped) gives the submit button a name=1, for an action.
    Form(IotsaWebContext& ctx, const IotsaWebLabel& submitLabel = "Save", const IotsaWebLabel& heading = "", const char *submitName = nullptr);
    ~Form();
  private:
    IotsaWebContext& _ctx;
  };

  // While it lives, every form the context writes gets this hidden field (a
  // collection's version, so a stale page is noticed).
  class FormHidden {
  public:
    FormHidden(IotsaWebContext& ctx, const char *name, const String& value);
    ~FormHidden();
  private:
    IotsaWebContext& _ctx;
    String _saved;
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
  // Never prefilled. clearable: with a "clear" checkbox (pass it only when
  // there is something to clear).
  void password(const IotsaWebLabel& label, const char *name, FieldKind kind = Config, const char *hint = nullptr, bool clearable = false);
  // A multi-line text field (a PEM key), never prefilled; makes the form POST.
  // Read it with password() on the put side.
  void textarea(const IotsaWebLabel& label, const char *name, FieldKind kind = Config, const char *hint = nullptr, bool clearable = false);
  // A <select> of count options; value is the index.
  void choice(const IotsaWebLabel& label, const char *name, int value, const char * const *options, int count, FieldKind kind = Config);
  // A button sending name=value. Inside a Form it is part of that form (and
  // sends its fields too), otherwise it gets a small form of its own.
  void button(const char *name, const IotsaWebLabel& label, const char *value = "1", FieldKind kind = Live);
  // A button with an "I am sure" box, for confirmedAction().
  void confirmedButton(const char *name, const IotsaWebLabel& label, FieldKind kind = Live);
  // A table cell, in formHandler_row(): <th>label</th> in the header row,
  // <td>value</td> in an element's row. Secret shows only whether it is set.
  enum CellFlags { Secret = 1 };
  void cell(const IotsaWebLabel& label, const String& value, int flags = 0);
  // Raw HTML, not escaped.
  IotsaWebContext& operator<<(const String& html);

  // Used by IotsaModule::webHandler().
  void sendPage(const String& title, const String& everyday, const String& configuration);
  // Used by a trigger: the feedback lines as plain text ("OK" if none), 400
  // if one of them is an error.
  void sendText();
  String takeOutput();

private:
  friend class IotsaWebCollection;
  struct Group {
    String html;
    String submitLabel;
    String submitName;
    String heading;
    bool editable = false;   // something can be submitted: write a form
    bool hasFields = false;  // ... and it needs a submit button
    bool post = false;
  };
  enum AuthRule { RestRule, NamedRight };

  bool _present(const char *name, String& value);
  bool _mayChange(FieldKind kind);
  void _changed(FieldKind kind);
  void _mark(FieldKind kind);
  void _field(const IotsaWebLabel& label, const char *name, const String& value, const char *type, FieldKind kind, const char *hint);
  String _scoped(const char *name) const;
  String& _out();
  void _markEditable(bool post, bool field = true);
  void _setRule(AuthRule rule, const char *right);
  void _closeForm();
  String _clearBox(const char *name);

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
  AuthRule _rule = RestRule;
  const char *_right = nullptr;
  bool _authChecked = false;
  bool _authOk = false;
  bool _inChange = false;   // reading a Change's parameters
  String _prefix;
  String _formHidden;       // hidden fields for every form, see FormHidden
  bool _headerRow = false;  // cell() writes <th>, see IotsaWebCollection
  String _output;
  std::vector<Group> _forms;
  struct Feedback {
    String text;
    bool error;
  };
  std::vector<Feedback> _feedback;
};

//
// A collection of objects on a web page (cwi-dis/iotsa#183): an interface on top
// of the module's own container. The container answers a few questions, the
// framework writes the table or the sections, the operation buttons and the add
// form, and handles them. The module calls it inside a Scope, like any object.
//
class IotsaWebCollection {
public:
  enum Ops { Add = 1, Remove = 2 };
  enum Presentation { Table, Sections };
  IotsaWebCollection(int ops, Presentation presentation, IotsaWebContext::FieldKind kind)
  : _ops(ops), _presentation(presentation), _kind(kind) {}
  virtual ~IotsaWebCollection() {}

  // ---- the container answers these ----
  virtual int size() = 0;
  virtual IotsaModObject* at(int i) = 0;
  // How an element is named in the page (and, cwi-dis/iotsa#217, in REST):
  // its name where it has one, else its index.
  virtual String key(int i) { return String(i); }
  // Call fn with a new, empty element: for the table header and the add form.
  virtual void withExample(std::function<void(IotsaModObject&)> fn) = 0;
  // Add the filled-in example, or refuse with ctx.error().
  virtual bool add(IotsaModObject& obj, IotsaWebContext& ctx) { return false; }
  virtual bool remove(int i) { return false; }

  // ---- written once, here ----
  void formHandler_args(IotsaWebContext& ctx);
  void formHandler_fields(IotsaWebContext& ctx);
protected:
  int _ops;
  Presentation _presentation;
  IotsaWebContext::FieldKind _kind;
  // Bumped on every add and remove, and in every form of the collection, so a
  // stale page (a reload, a bookmark, another browser) changes nothing.
  // Random at boot, so a page from before a reboot is stale too.
  uint32_t _version = 0;
  uint32_t _currentVersion();
};

// The common case: a std::vector of values (users, tokens). accept, if given,
// checks (and may complete) a new element before it is added.
template<typename T> class IotsaWebVector : public IotsaWebCollection {
public:
  typedef std::function<String(T&)> KeyFunc;
  typedef std::function<bool(T&, IotsaWebContext&)> AcceptFunc;
  IotsaWebVector(std::vector<T>& v, int ops, Presentation presentation, IotsaWebContext::FieldKind kind, KeyFunc keyFunc = nullptr, AcceptFunc accept = nullptr)
  : IotsaWebCollection(ops, presentation, kind), _v(v), _keyFunc(keyFunc), _accept(accept) {}
  int size() override { return _v.size(); }
  IotsaModObject* at(int i) override { return &_v[i]; }
  String key(int i) override { return _keyFunc ? _keyFunc(_v[i]) : String(i); }
  void withExample(std::function<void(IotsaModObject&)> fn) override { T example; fn(example); }
  bool add(IotsaModObject& obj, IotsaWebContext& ctx) override {
    T& t = static_cast<T&>(obj);
    if (_accept && !_accept(t, ctx)) return false;
    _v.push_back(t);
    return true;
  }
  bool remove(int i) override { _v.erase(_v.begin() + i); return true; }
private:
  std::vector<T>& _v;
  KeyFunc _keyFunc;
  AcceptFunc _accept;
};

#endif // IOTSA_WITH_WEB
#endif
