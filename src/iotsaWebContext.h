#ifndef _IOTSAWEBCONTEXT_H_
#define _IOTSAWEBCONTEXT_H_
#include "iotsa.h"
#include "iotsaContext.h"

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

class IotsaWebContext : public IotsaHttpContext {
public:
  IotsaWebContext(IotsaBaseModule& mod, IotsaWebServer *server, const String& apiPath);

  // ---- put side ----

  // If the argument is present and differs from field: check auth (once per
  // request), check configuration mode, parse, assign. Returns true if field
  // changed. A refused or unparseable change is an error, which stops the
  // rest of the request (see IotsaContext::error()).
  template<typename T> bool arg(const char *name, T& field, Persistence persistence) {
    String value;
    if (!_present(name, value)) return false;
    T newValue;
    if (!_parse(value, newValue)) {
      error(String("Invalid value for ") + _scoped(name) + ".");
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
  // A checkbox (written by showCheckbox() on the get side, with its hidden 0).
  bool argCheckbox(const char *name, bool& field, Persistence persistence) { return arg(name, field, persistence); }
  // A password: an empty one means "leave unchanged", unless allowEmpty (an
  // open network, but only together with a new network name). clearable: an
  // optional secret, its "clear" checkbox empties it.
  bool argPassword(const char *name, String& field, Persistence persistence, bool allowEmpty = false, bool clearable = false);
  // One of count choices, 0..count-1 (written by showChoice() on the get side).
  bool argChoice(const char *name, int& field, int count, Persistence persistence);
  // A button without fields (identify, reboot, remove): present and allowed.
  // Counts as a change of the given persistence.
  bool argAction(const char *name, Persistence persistence);
  // Read an argument without any check or effect (hidden bookkeeping fields,
  // a collection's version). It does count as used.
  bool argPeek(const char *name, String& value) { return _present(name, value); }
  // A button that needs its "I am sure" box ticked (factory reset, remove all).
  bool argConfirmedAction(const char *name);

  // Lines shown at the top of the page.
  void feedback(const String& text);
  bool hasFeedback() const { return !_feedback.empty(); }
  // An error line at the top of the page: IotsaContext::error() with 400.
  using IotsaContext::error;
  void error(const String& text) { IotsaContext::error(400, text); }
  // Answer the request yourself (plain text, 404): the framework sends nothing more.
  void respond(int code, const char *type, const String& body);
  // After the put side: request arguments nothing read become a feedback
  // line, unless the request failed (then later fields weren't read anyway).
  void reportUnused();

  bool responded = false;         // auth failed (challenge sent), or respond() was called

  // ---- objects inside a module ----

  // While a Scope lives, names get "<prefix>." in front, so an object writes
  // ctx.arg("url", url) and the page sends "button1.url". The rights the
  // object declares end with the Scope, as in IotsaApiContext::Scope.
  class Scope {
  public:
    Scope(IotsaWebContext& ctx, const String& prefix);
    ~Scope();
  private:
    IotsaWebContext& _ctx;
    String _saved;
    _Rights _savedRights;
  };

  // ---- get side ----

  // Which parts of the module's interface this request covers. The page
  // calls formHandler_fields() twice: once for the everyday part, once for
  // the settings.
  bool includesEveryday() const { return _includesEveryday; }
  bool includesSettings() const { return _includesSettings; }
  // Whether a field of this persistence can be edited now: the mode allows
  // it, and the write right is granted or can be obtained by logging in.
  // When not, the part of the page gets a line saying why.
  bool editable(Persistence persistence);

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
  // or "" when it has no REST counterpart. Like the other show*() calls, it
  // shows "(hidden)" when showsNeedRight() names a right the caller doesn't
  // have; with no credentials at all, the page asks for them instead.
  void showStatus(const IotsaWebLabel& label, const char *name, const String& value);
  // An input when editable, otherwise the same as showStatus().
  void showField(const IotsaWebLabel& label, const char *name, const String& value, Persistence persistence, const char *hint = nullptr);
  void showField(const IotsaWebLabel& label, const char *name, const char *value, Persistence persistence, const char *hint = nullptr) { showField(label, name, String(value), persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, int value, Persistence persistence, const char *hint = nullptr) { _field(label, name, String(value), "number", persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, unsigned int value, Persistence persistence, const char *hint = nullptr) { _field(label, name, String(value), "number", persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, long value, Persistence persistence, const char *hint = nullptr) { _field(label, name, String(value), "number", persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, unsigned long value, Persistence persistence, const char *hint = nullptr) { _field(label, name, String(value), "number", persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, float value, Persistence persistence, const char *hint = nullptr) { _field(label, name, String(value), "number' step='any", persistence, hint); }
  void showField(const IotsaWebLabel& label, const char *name, bool value, Persistence persistence, const char *hint = nullptr) { showCheckbox(label, name, value, persistence, hint); }
  void showCheckbox(const IotsaWebLabel& label, const char *name, bool value, Persistence persistence, const char *hint = nullptr);
  // Never prefilled. clearable: with a "clear" checkbox (pass it only when
  // there is something to clear).
  void showPassword(const IotsaWebLabel& label, const char *name, Persistence persistence, const char *hint = nullptr, bool clearable = false);
  // A multi-line text field (a PEM key), never prefilled; makes the form POST.
  // Read it with argPassword() on the put side.
  void showTextarea(const IotsaWebLabel& label, const char *name, Persistence persistence, const char *hint = nullptr, bool clearable = false);
  // A <select> of count options; value is the index.
  void showChoice(const IotsaWebLabel& label, const char *name, int value, const char * const *options, int count, Persistence persistence);
  // A button sending name=value. Inside a Form it is part of that form (and
  // sends its fields too), otherwise it gets a small form of its own.
  void showButton(const char *name, const IotsaWebLabel& label, Persistence persistence, const char *value = "1");
  // A button with an "I am sure" box, for argConfirmedAction().
  void showConfirmedButton(const char *name, const IotsaWebLabel& label, Persistence persistence);
  // A table cell, in formHandler_row(): <th>label</th> in the header row,
  // <td>value</td> in an element's row. Secret shows only whether it is set.
  enum CellFlags { Secret = 1 };
  void showCell(const IotsaWebLabel& label, const String& value, int flags = 0);
  // Raw HTML, not escaped.
  IotsaWebContext& operator<<(const String& html);

  // Used by IotsaModule::webHandler().
  void setParts(bool everyday, bool settings) { _includesEveryday = everyday; _includesSettings = settings; }
  void sendPage(const String& title, const String& everyday, const String& settings);
  // Used by a trigger: the feedback lines as plain text ("OK" if none), or
  // the error with its status.
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
  bool _present(const char *name, String& value);
  bool _hasArg(const char *name) override { String value; return _present(name, value); }
  void _authRefused() override { responded = true; }
  void _field(const IotsaWebLabel& label, const char *name, const String& value, const char *type, Persistence persistence, const char *hint);
  String _scoped(const char *name) const;
  // May the caller read the value (showsNeedRight())? If not, the part gets
  // a line saying some values are hidden.
  bool _readable(bool empty);
  String& _out();
  void _markEditable(bool post, bool field = true);
  void _closeForm();
  String _clearBox(const char *name);

  static bool _parse(const String& s, String& v) { v = s; return true; }
  static bool _parse(const String& s, bool& v);
  static bool _parse(const String& s, int& v);
  static bool _parse(const String& s, unsigned int& v);
  static bool _parse(const String& s, long& v);
  static bool _parse(const String& s, unsigned long& v);
  static bool _parse(const String& s, float& v);

  bool _includesEveryday = true;
  bool _includesSettings = true;
  bool _plainForMode = false;   // a field was plain text: configuration mode
  bool _plainForRight = false;  // ... or a right the caller doesn't have
  bool _hiddenForRight = false; // a value was hidden: a right to read it is missing
  String _prefix;
  String _formHidden;       // hidden fields for every form, see FormHidden
  bool _headerRow = false;  // showCell() writes <th>, see IotsaWebCollection
  String _output;
  std::vector<Group> _forms;
  struct Feedback {
    String text;
  };
  std::vector<Feedback> _feedback;
};

//
// A collection of objects on a web page (cwi-dis/iotsa#183): an interface on top
// of the module's own container. The container answers a few questions, the
// framework writes the table or the sections, the operation buttons and the add
// form, and handles them. The module calls it inside a Scope, like any object.
//
// There is no Reorder on purpose: no collection has an order that means
// something (cwi-dis/iotsa433#2), and a REST PUT of the whole list can reorder.
//
class IotsaWebCollection {
public:
  enum Ops { Add = 1, Remove = 2 };
  enum Presentation { Table, Sections };
  IotsaWebCollection(int ops, Presentation presentation, IotsaWebContext::Persistence persistence)
  : _ops(ops), _presentation(presentation), _persistence(persistence) {}
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
  IotsaWebContext::Persistence _persistence;
  // Bumped on every add and remove, and in every form of the collection, so a
  // stale page (a reload, a bookmark, another browser) changes nothing.
  // Random at boot, so a page from before a reboot is stale too.
  uint32_t _version = 0;
  uint32_t _currentVersion();
};

// The common case: a std::vector of values (users, tokens). A module
// subclasses it and names it after its elements, "<Element>List"
// (IotsaUserList), overriding only what differs: key() when the elements
// have a name, accept() to check (and complete) a new element before it is
// added. Not "Web" in the name: the same list is meant to serve the API too
// one day (cwi-dis/iotsa#309).
template<typename T> class IotsaWebVector : public IotsaWebCollection {
public:
  IotsaWebVector(std::vector<T>& v, int ops, Presentation presentation, IotsaWebContext::Persistence persistence)
  : IotsaWebCollection(ops, presentation, persistence), _v(v) {}
  int size() override { return _v.size(); }
  IotsaModObject* at(int i) override { return &_v[i]; }
  void withExample(std::function<void(IotsaModObject&)> fn) override { T example; fn(example); }
  bool add(IotsaModObject& obj, IotsaWebContext& ctx) override {
    T& t = static_cast<T&>(obj);
    if (!accept(t, ctx)) return false;
    _v.push_back(t);
    return true;
  }
  bool remove(int i) override { _v.erase(_v.begin() + i); return true; }
protected:
  // A new element: refuse it with ctx.error() and false. Default: accepted.
  virtual bool accept(T& t, IotsaWebContext& ctx) { return true; }
  std::vector<T>& _v;
};

#endif // IOTSA_WITH_WEB
#endif
