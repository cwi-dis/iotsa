#include "iotsaApi.h"

#ifdef IOTSA_WITH_WEB
#include <stdlib.h>
#include <errno.h>

static const char *_pageStyle =
  "body{font-family:sans-serif;max-width:40em;margin:0 auto;padding:0 1em}\n"
  "label{display:block;margin:0.3em 0}\n"
  "small{color:#666}\n"
  ".error{color:#b00}\n"
  "fieldset{margin:0.5em 0}\n"
  "form.button{display:inline-block;margin:0.2em 0.5em 0.2em 0}\n"
  "table{border-collapse:collapse}\n"
  "th,td{border:1px solid #888;padding:0.2em 0.5em}\n";

IotsaWebContext::IotsaWebContext(IotsaBaseModule& mod, IotsaWebServer *server, const String& apiPath)
: _mod(mod),
  _server(server),
  _apiPath(apiPath)
{
}

//
// Put side
//

bool IotsaWebContext::_present(const char *name, String& value) {
  if (responded) return false;
  String scoped = _scoped(name);
  if (!_server->hasArg(scoped)) return false;
  value = _server->arg(scoped);
  return true;
}

bool IotsaWebContext::_mayChange(FieldKind kind) {
  if (responded) return false;
  // A Change's parameters: the Change itself was checked.
  if (_inChange) return true;
  if (!_authChecked) {
    _authChecked = true;
    // Both send their own challenge when they refuse.
    if (_rule == NamedRight) {
      _authOk = !_mod.needsAuthentication(_right);
    } else {
      _authOk = !_mod.needsAuthentication(_apiPath.c_str(), IOTSA_API_PUT);
    }
  }
  if (!_authOk) {
    responded = true;
    return false;
  }
  if (!editable(kind)) {
    if (!refusedConfigMode) error("Some settings can only be changed in configuration mode.");
    refusedConfigMode = true;
    return false;
  }
  return true;
}

void IotsaWebContext::_setRule(AuthRule rule, const char *right) {
  bool sameRight = right == _right || (right && _right && strcmp(right, _right) == 0);
  if (rule == _rule && (rule != NamedRight || sameRight)) return;
  _rule = rule;
  _right = right;
  // A refusal has already answered the request, so only a successful (or not
  // yet done) check is redone for the new rule.
  if (_authOk || !_authChecked) _authChecked = false;
}

void IotsaWebContext::_changed(FieldKind kind) {
  // A Change's parameters only read: Change::done() marks.
  if (_inChange) return;
  _mark(kind);
}

void IotsaWebContext::_mark(FieldKind kind) {
  if (kind == Live) {
    stateChanged = true;
  } else {
    configChanged = true;
  }
}

bool IotsaWebContext::password(const char *name, String& field, FieldKind kind, bool allowEmpty) {
  String value;
  if (!_present(name, value)) return false;
  if ((value == "" && !allowEmpty) || value == field) return false;
  if (!_mayChange(kind)) return false;
  field = value;
  _changed(kind);
  return true;
}

bool IotsaWebContext::choice(const char *name, int& field, int count, FieldKind kind) {
  String value;
  if (!_present(name, value)) return false;
  int newValue;
  if (!_parse(value, newValue) || newValue < 0 || newValue >= count) {
    error(String("Invalid value for ") + _scoped(name) + ".");
    return false;
  }
  if (newValue == field) return false;
  if (!_mayChange(kind)) return false;
  field = newValue;
  _changed(kind);
  return true;
}

bool IotsaWebContext::action(const char *name, FieldKind kind) {
  String value;
  if (!_present(name, value)) return false;
  if (!_mayChange(kind)) return false;
  _changed(kind);
  return true;
}

bool IotsaWebContext::text(const char *name, String& value, FieldKind kind) {
  String raw;
  if (!_present(name, raw) || raw == "") return false;
  if (!_mayChange(kind)) return false;
  value = raw;
  return true;
}

bool IotsaWebContext::confirmedAction(const char *name) {
  String value;
  if (!_present(name, value)) return false;
  String sure;
  if (!_present((String(name) + ".sure").c_str(), sure) || sure != "1") {
    error("Not done: tick \"I am sure\" first.");
    return false;
  }
  if (!_mayChange(Live)) return false;
  stateChanged = true;
  return true;
}

IotsaWebContext::Change::Change(IotsaWebContext& ctx, const char *button, FieldKind kind)
: _ctx(ctx),
  _kind(kind),
  _outer(ctx._inChange)
{
  String value;
  if (!_ctx._present(button, value)) return;
  if (!_ctx._mayChange(kind)) return;
  _active = true;
  _ctx._inChange = true;
}

IotsaWebContext::Change::~Change() {
  _ctx._inChange = _outer;
}

void IotsaWebContext::Change::done() {
  if (_active && !_outer) _ctx._mark(_kind);
}

void IotsaWebContext::feedback(const String& text) {
  _feedback.push_back("<p>" + IotsaBaseModule::htmlEncode(text) + "</p>\n");
}

void IotsaWebContext::error(const String& text) {
  _feedback.push_back("<p class='error'>" + IotsaBaseModule::htmlEncode(text) + "</p>\n");
}

void IotsaWebContext::respond(int code, const char *type, const String& body) {
  _server->send(code, type, body);
  responded = true;
}

bool IotsaWebContext::_parse(const String& s, bool& v) {
  if (s == "1" || s == "true" || s == "on") { v = true; return true; }
  if (s == "0" || s == "false" || s == "") { v = false; return true; }
  return false;
}

bool IotsaWebContext::_parse(const String& s, long& v) {
  char *end;
  errno = 0;
  v = strtol(s.c_str(), &end, 0);
  return s.length() > 0 && *end == '\0' && errno == 0;
}

bool IotsaWebContext::_parse(const String& s, unsigned long& v) {
  char *end;
  errno = 0;
  if (s.startsWith("-")) return false;
  v = strtoul(s.c_str(), &end, 0);
  return s.length() > 0 && *end == '\0' && errno == 0;
}

bool IotsaWebContext::_parse(const String& s, int& v) {
  long l;
  if (!_parse(s, l) || l < INT_MIN || l > INT_MAX) return false;
  v = l;
  return true;
}

bool IotsaWebContext::_parse(const String& s, unsigned int& v) {
  unsigned long l;
  if (!_parse(s, l) || l > UINT_MAX) return false;
  v = l;
  return true;
}

bool IotsaWebContext::_parse(const String& s, float& v) {
  char *end;
  v = strtof(s.c_str(), &end);
  return s.length() > 0 && *end == '\0';
}

//
// Objects inside a module
//

IotsaWebContext::Scope::Scope(IotsaWebContext& ctx, const String& prefix)
: _ctx(ctx),
  _saved(ctx._prefix)
{
  _ctx._prefix = _saved + prefix + ".";
}

IotsaWebContext::Scope::~Scope() {
  _ctx._prefix = _saved;
}

String IotsaWebContext::_scoped(const char *name) const {
  return _prefix + name;
}

//
// Get side
//

bool IotsaWebContext::editable(FieldKind kind) const {
  if (kind == ConfigModeOnly) return iotsaConfigSettingsWritable();
  if (kind == ConfigModeStrict) return iotsaController.inConfigurationMode();
  return true;
}

String& IotsaWebContext::_out() {
  if (_forms.empty()) return _output;
  return _forms.back().html;
}

IotsaWebContext& IotsaWebContext::operator<<(const String& html) {
  _out() += html;
  return *this;
}

void IotsaWebContext::_markEditable(bool post, bool field) {
  // An editable field makes the outermost form, which is the one that is
  // written as a <form>, editable too.
  for (auto& f : _forms) {
    f.editable = true;
    if (field) f.hasFields = true;
    if (post) f.post = true;
  }
}

IotsaWebContext::Form::Form(IotsaWebContext& ctx, const IotsaWebLabel& submitLabel, const IotsaWebLabel& heading, const char *submitName)
: _ctx(ctx)
{
  Group g;
  g.submitLabel = submitLabel.text;
  g.heading = heading.text;
  if (submitName) g.submitName = _ctx._scoped(submitName);
  _ctx._forms.push_back(g);
}

IotsaWebContext::FormHidden::FormHidden(IotsaWebContext& ctx, const char *name, const String& value)
: _ctx(ctx),
  _saved(ctx._formHidden)
{
  _ctx._formHidden += "<input type='hidden' name='" + _ctx._scoped(name) + "' value='" + IotsaBaseModule::htmlEncode(value) + "'>";
}

IotsaWebContext::FormHidden::~FormHidden() {
  _ctx._formHidden = _saved;
}

IotsaWebContext::Form::~Form() {
  _ctx._closeForm();
}

void IotsaWebContext::_closeForm() {
  Group g = _forms.back();
  _forms.pop_back();
  String heading = IotsaBaseModule::htmlEncode(g.heading);
  if (g.html.length() == 0) {
    // Nothing in it (all fields hidden outside configuration mode): no heading either.
    return;
  }
  if (!g.editable) {
    // Nothing to submit: no form, no button.
    if (heading.length()) _out() += "<h3>" + heading + "</h3>\n";
    _out() += g.html;
  } else if (!_forms.empty()) {
    // A form is already open: become part of it.
    _out() += "<fieldset>\n";
    if (heading.length()) _out() += "<legend>" + heading + "</legend>\n";
    _out() += g.html + "</fieldset>\n";
  } else {
    if (heading.length()) _out() += "<h3>" + heading + "</h3>\n";
    _out() += String("<form method='") + (g.post ? "post" : "get") + "'>" + _formHidden + "\n";
    _out() += g.html;
    if (g.hasFields) {
      _out() += "<button type='submit'";
      if (g.submitName.length()) _out() += " name='" + g.submitName + "' value='1'";
      _out() += ">" + IotsaBaseModule::htmlEncode(g.submitLabel) + "</button>\n";
    }
    _out() += "</form>\n";
  }
}

void IotsaWebContext::status(const IotsaWebLabel& label, const char *name, const String& value) {
  String line = "<div";
  if (name && *name) line += " id='" + _scoped(name) + "'";
  line += ">" + IotsaBaseModule::htmlEncode(label.text) + ": " + IotsaBaseModule::htmlEncode(value) + "</div>\n";
  _out() += line;
}

void IotsaWebContext::field(const IotsaWebLabel& label, const char *name, const String& value, FieldKind kind, const char *hint) {
  _field(label, name, value, "text", kind, hint);
}

void IotsaWebContext::_field(const IotsaWebLabel& label, const char *name, const String& value, const char *type, FieldKind kind, const char *hint) {
  if (!editable(kind)) {
    status(label, name, value);
    return;
  }
  _markEditable(false);
  String line = "<label>" + IotsaBaseModule::htmlEncode(label.text) + ": ";
  line += String("<input type='") + type + "' name='" + _scoped(name) + "' value='" + IotsaBaseModule::htmlEncode(value) + "'>";
  if (hint) line += String(" <small>") + IotsaBaseModule::htmlEncode(hint) + "</small>";
  line += "</label>\n";
  _out() += line;
}

void IotsaWebContext::checkbox(const IotsaWebLabel& label, const char *name, bool value, FieldKind kind, const char *hint) {
  if (!editable(kind)) {
    status(label, name, value ? "yes" : "no");
    return;
  }
  _markEditable(false);
  String scoped = _scoped(name);
  // The hidden 0 comes after the checkbox: the servers return the first value,
  // so an unchecked box still sends a 0.
  String line = "<label><input type='checkbox' name='" + scoped + "' value='1'";
  if (value) line += " checked";
  line += "> " + IotsaBaseModule::htmlEncode(label.text);
  if (hint) line += String(" <small>") + IotsaBaseModule::htmlEncode(hint) + "</small>";
  line += "</label><input type='hidden' name='" + scoped + "' value='0'>\n";
  _out() += line;
}

void IotsaWebContext::password(const IotsaWebLabel& label, const char *name, FieldKind kind, const char *hint) {
  if (!editable(kind)) return;
  _markEditable(true);
  String line = "<label>" + IotsaBaseModule::htmlEncode(label.text) + ": ";
  line += "<input type='password' name='" + _scoped(name) + "' value=''>";
  if (hint) line += String(" <small>") + IotsaBaseModule::htmlEncode(hint) + "</small>";
  line += "</label>\n";
  _out() += line;
}

void IotsaWebContext::textarea(const IotsaWebLabel& label, const char *name, FieldKind kind, const char *hint) {
  if (!editable(kind)) return;
  _markEditable(true);
  String line = "<label>" + IotsaBaseModule::htmlEncode(label.text) + ":";
  if (hint) line += String(" <small>") + IotsaBaseModule::htmlEncode(hint) + "</small>";
  line += "<br><textarea name='" + _scoped(name) + "' rows='6' cols='64'></textarea></label>\n";
  _out() += line;
}

void IotsaWebContext::choice(const IotsaWebLabel& label, const char *name, int value, const char * const *options, int count, FieldKind kind) {
  if (!editable(kind)) {
    status(label, name, (value >= 0 && value < count) ? options[value] : String(value));
    return;
  }
  _markEditable(false);
  String line = "<label>" + IotsaBaseModule::htmlEncode(label.text) + ": <select name='" + _scoped(name) + "'>";
  for (int i = 0; i < count; i++) {
    line += "<option value='" + String(i) + "'" + (i == value ? " selected" : "") + ">" + IotsaBaseModule::htmlEncode(options[i]) + "</option>";
  }
  line += "</select></label>\n";
  _out() += line;
}

void IotsaWebContext::button(const char *name, const IotsaWebLabel& label, const char *value, FieldKind kind) {
  if (!editable(kind)) return;
  String html = "<button type='submit' name='" + _scoped(name) + "' value='" + IotsaBaseModule::htmlEncode(value) + "'>" + IotsaBaseModule::htmlEncode(label.text) + "</button>";
  if (_forms.empty()) {
    _out() += "<form method='post' class='button'>" + _formHidden + html + "</form>\n";
  } else {
    _markEditable(true, false);
    _out() += html + "\n";
  }
}

void IotsaWebContext::confirmedButton(const char *name, const IotsaWebLabel& label, FieldKind kind) {
  if (!editable(kind)) return;
  String html = "<label><input type='checkbox' name='" + _scoped(name) + ".sure' value='1'> I am sure</label>";
  html += "<button type='submit' name='" + _scoped(name) + "' value='1'>" + IotsaBaseModule::htmlEncode(label.text) + "</button>";
  if (_forms.empty()) {
    _out() += "<form method='post' class='button'>" + _formHidden + html + "</form>\n";
  } else {
    _markEditable(true, false);
    _out() += html + "\n";
  }
}

void IotsaWebContext::cell(const IotsaWebLabel& label, const String& value, int flags) {
  if (_headerRow) {
    _out() += "<th>" + IotsaBaseModule::htmlEncode(label.text) + "</th>";
  } else if (flags & Secret) {
    _out() += String("<td>") + (value.length() ? "(set)" : "") + "</td>";
  } else {
    _out() += "<td>" + IotsaBaseModule::htmlEncode(value) + "</td>";
  }
}

String IotsaWebContext::takeOutput() {
  String rv = _output;
  _output = "";
  return rv;
}

void IotsaWebContext::sendPage(const String& title, const String& everyday, const String& configuration) {
  String page = "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>\n";
  page += "<title>" + IotsaBaseModule::htmlEncode(title) + "</title>\n";
  page += String("<style>\n") + _pageStyle + "</style></head><body>\n";
  page += "<h1>" + IotsaBaseModule::htmlEncode(title) + "</h1>\n";
  for (auto& f : _feedback) page += f;
  page += everyday;
  if (configuration.length() > 0) {
    page += "<h2>Configuration</h2>\n";
    page += configuration;
  }
  page += "<p><a href='/'>Home</a></p>\n</body></html>\n";
  _server->send(200, "text/html", page);
}

//
// IotsaWebCollection
//

uint32_t IotsaWebCollection::_currentVersion() {
  if (_version == 0) {
#ifdef ESP32
    _version = esp_random() | 1;
#else
    _version = RANDOM_REG32 | 1;
#endif
  }
  return _version;
}

void IotsaWebCollection::formHandler_args(IotsaWebContext& ctx) {
  String v;
  if (ctx.peek("version", v) && v != String(_currentVersion())) {
    ctx.error("This page was out of date, nothing changed. Reload it and try again.");
    return;
  }
  if (_ops & Remove) {
    for (int i = 0; i < size(); i++) {
      IotsaWebContext::Scope s(ctx, key(i));
      IotsaWebContext::Change change(ctx, "remove", _kind);
      if (change) {
        // The indices shift: nothing else in this request.
        if (remove(i)) {
          _version++;
          change.done();
        }
        return;
      }
    }
  }
  if (_ops & Add) {
    IotsaWebContext::Scope s(ctx, "new");
    IotsaWebContext::Change change(ctx, "add", _kind);
    if (change) {
      withExample([&](IotsaModObject& obj) {
        obj.formHandler_args(ctx);
        if (add(obj, ctx)) {
          _version++;
          change.done();
        }
      });
      return;
    }
  }
  if (_presentation == Sections) {
    for (int i = 0; i < size(); i++) {
      IotsaWebContext::Scope s(ctx, key(i));
      at(i)->formHandler_args(ctx);
    }
  }
}

void IotsaWebCollection::formHandler_fields(IotsaWebContext& ctx) {
  IotsaWebContext::FormHidden version(ctx, "version", String(_currentVersion()));
  bool removable = (_ops & Remove) && ctx.editable(_kind);
  if (_presentation == Table) {
    ctx << "<table>\n<tr>";
    ctx._headerRow = true;
    withExample([&](IotsaModObject& obj) { obj.formHandler_row(ctx); });
    ctx._headerRow = false;
    if (removable) ctx << "<th></th>";
    ctx << "</tr>\n";
    for (int i = 0; i < size(); i++) {
      IotsaWebContext::Scope s(ctx, key(i));
      ctx << "<tr>";
      at(i)->formHandler_row(ctx);
      if (removable) {
        ctx << "<td>";
        ctx.button("remove", "Remove", "1", _kind);
        ctx << "</td>";
      }
      ctx << "</tr>\n";
    }
    ctx << "</table>\n";
  } else {
    for (int i = 0; i < size(); i++) {
      String k = key(i);
      IotsaWebContext::Scope s(ctx, k);
      {
        IotsaWebContext::Form form(ctx, "Change", k);
        at(i)->formHandler_fields(ctx);
      }
      if (removable) ctx.button("remove", "Remove", "1", _kind);
    }
  }
  if ((_ops & Add) && ctx.editable(_kind)) {
    IotsaWebContext::Scope s(ctx, "new");
    IotsaWebContext::Form form(ctx, "Add", "Add", "add");
    withExample([&](IotsaModObject& obj) { obj.formHandler_fields(ctx); });
  }
}

#endif // IOTSA_WITH_WEB
