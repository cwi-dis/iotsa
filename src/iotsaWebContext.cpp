#include "iotsaApi.h"

#ifdef IOTSA_WITH_WEB
#include <stdlib.h>
#include <errno.h>

// No styling on the pages, on purpose: they read the same in a text browser,
// and global styling can be added later without overrides here. Each field
// is a <div> of its own, so it gets a line of its own. The class names
// (error, button) are hooks for that styling.

IotsaWebContext::IotsaWebContext(IotsaBaseModule& mod, IotsaWebServer *server, const String& apiPath)
: IotsaHttpContext(mod, server, apiPath)
{
}

//
// Put side
//

bool IotsaWebContext::_present(const char *name, String& value) {
  if (responded || failed()) return false;
  String scoped = _scoped(name);
  if (!_server->hasArg(scoped)) return false;
  _use(scoped);
  value = _server->arg(scoped);
  return true;
}

bool IotsaWebContext::argPassword(const char *name, String& field, Persistence persistence, bool allowEmpty, bool clearable) {
  String value;
  if (clearable && _present((String(name) + ".clear").c_str(), value) && value == "1") {
    if (field == "" || !_mayChange(persistence)) return false;
    field = "";
    _changed(persistence);
    return true;
  }
  if (!_present(name, value)) return false;
  if (value == "" && !allowEmpty) return false;
  // Check before comparing: a password is never readable, so "unchanged, not
  // refused" must not tell a right guess from a wrong one.
  if (!_mayChange(persistence)) return false;
  if (value == field) return false;
  field = value;
  _changed(persistence);
  return true;
}

bool IotsaWebContext::argChoice(const char *name, int& field, int count, Persistence persistence) {
  String value;
  if (!_present(name, value)) return false;
  int newValue;
  if (!_parse(value, newValue) || newValue < 0 || newValue >= count) {
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

bool IotsaWebContext::argAction(const char *name, Persistence persistence) {
  String value;
  if (!_present(name, value)) return false;
  if (!_mayChange(persistence)) return false;
  _changed(persistence);
  return true;
}

bool IotsaWebContext::argConfirmedAction(const char *name) {
  String value;
  if (!_present(name, value)) return false;
  String sure;
  if (!_present((String(name) + ".sure").c_str(), sure) || sure != "1") {
    error("Not done: tick \"I am sure\" first.");
    return false;
  }
  if (!_mayChange(Transient)) return false;
  _changed(Transient);
  return true;
}

void IotsaWebContext::feedback(const String& text) {
  _feedback.push_back({text});
}

void IotsaWebContext::respond(int code, const char *type, const String& body) {
  _server->send(code, type, body);
  responded = true;
}

void IotsaWebContext::reportUnused() {
  if (responded || failed()) return;
  String names;
  for (int i = 0; i < _server->args(); i++) {
    String name = _server->argName(i);
    // "plain": the request body, added by the ESP32 web server itself.
    if (name == "plain" || _isUsed(name)) continue;
    _use(name);  // once, also when sent twice
    if (names.length()) names += ", ";
    names += name;
  }
  if (names.length()) feedback("Ignored: " + names + ".");
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

bool IotsaWebContext::editable(Persistence persistence) {
  if (_writeAccess() == IotsaAccess::Forbidden) {
    _plainForRight = true;
    return false;
  }
  if (!_modeAllows(persistence)) {
    _plainForMode = true;
    return false;
  }
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

void IotsaWebContext::showStatus(const IotsaWebLabel& label, const char *name, const String& value) {
  String line = "<div";
  if (name && *name) line += " id='" + _scoped(name) + "'";
  line += ">" + IotsaBaseModule::htmlEncode(label.text) + ": " + IotsaBaseModule::htmlEncode(value) + "</div>\n";
  _out() += line;
}

void IotsaWebContext::showField(const IotsaWebLabel& label, const char *name, const String& value, Persistence persistence, const char *hint) {
  _field(label, name, value, "text", persistence, hint);
}

void IotsaWebContext::_field(const IotsaWebLabel& label, const char *name, const String& value, const char *type, Persistence persistence, const char *hint) {
  if (!editable(persistence)) {
    showStatus(label, name, value);
    return;
  }
  _markEditable(false);
  String line = "<div><label>" + IotsaBaseModule::htmlEncode(label.text) + ": ";
  line += String("<input type='") + type + "' name='" + _scoped(name) + "' value='" + IotsaBaseModule::htmlEncode(value) + "'>";
  if (hint) line += String(" <small>(") + IotsaBaseModule::htmlEncode(hint) + ")</small>";
  line += "</label></div>\n";
  _out() += line;
}

void IotsaWebContext::showCheckbox(const IotsaWebLabel& label, const char *name, bool value, Persistence persistence, const char *hint) {
  if (!editable(persistence)) {
    showStatus(label, name, value ? "yes" : "no");
    return;
  }
  _markEditable(false);
  String scoped = _scoped(name);
  // The hidden 0 comes after the checkbox: the servers return the first value,
  // so an unchecked box still sends a 0.
  String line = "<div><label><input type='checkbox' name='" + scoped + "' value='1'";
  if (value) line += " checked";
  line += "> " + IotsaBaseModule::htmlEncode(label.text);
  if (hint) line += String(" <small>(") + IotsaBaseModule::htmlEncode(hint) + ")</small>";
  line += "</label><input type='hidden' name='" + scoped + "' value='0'></div>\n";
  _out() += line;
}

String IotsaWebContext::_clearBox(const char *name) {
  return "<div><label><input type='checkbox' name='" + _scoped(name) + ".clear' value='1'> Clear</label></div>\n";
}

void IotsaWebContext::showPassword(const IotsaWebLabel& label, const char *name, Persistence persistence, const char *hint, bool clearable) {
  if (!editable(persistence)) return;
  _markEditable(true);
  String line = "<div><label>" + IotsaBaseModule::htmlEncode(label.text) + ": ";
  line += "<input type='password' name='" + _scoped(name) + "' value=''>";
  if (hint) line += String(" <small>(") + IotsaBaseModule::htmlEncode(hint) + ")</small>";
  line += "</label></div>\n";
  if (clearable) line += _clearBox(name);
  _out() += line;
}

void IotsaWebContext::showTextarea(const IotsaWebLabel& label, const char *name, Persistence persistence, const char *hint, bool clearable) {
  if (!editable(persistence)) return;
  _markEditable(true);
  String line = "<div><label>" + IotsaBaseModule::htmlEncode(label.text) + ":";
  if (hint) line += String(" <small>(") + IotsaBaseModule::htmlEncode(hint) + ")</small>";
  line += "<br><textarea name='" + _scoped(name) + "' rows='6' cols='64'></textarea></label></div>\n";
  if (clearable) line += _clearBox(name);
  _out() += line;
}

void IotsaWebContext::showChoice(const IotsaWebLabel& label, const char *name, int value, const char * const *options, int count, Persistence persistence) {
  if (!editable(persistence)) {
    showStatus(label, name, (value >= 0 && value < count) ? options[value] : String(value));
    return;
  }
  _markEditable(false);
  String line = "<div><label>" + IotsaBaseModule::htmlEncode(label.text) + ": <select name='" + _scoped(name) + "'>";
  for (int i = 0; i < count; i++) {
    line += "<option value='" + String(i) + "'" + (i == value ? " selected" : "") + ">" + IotsaBaseModule::htmlEncode(options[i]) + "</option>";
  }
  line += "</select></label></div>\n";
  _out() += line;
}

void IotsaWebContext::showButton(const char *name, const IotsaWebLabel& label, Persistence persistence, const char *value) {
  if (!editable(persistence)) return;
  String html = "<button type='submit' name='" + _scoped(name) + "' value='" + IotsaBaseModule::htmlEncode(value) + "'>" + IotsaBaseModule::htmlEncode(label.text) + "</button>";
  if (_forms.empty()) {
    _out() += "<form method='post' class='button'>" + _formHidden + html + "</form>\n";
  } else {
    _markEditable(true, false);
    _out() += html + "\n";
  }
}

void IotsaWebContext::showConfirmedButton(const char *name, const IotsaWebLabel& label, Persistence persistence) {
  if (!editable(persistence)) return;
  String html = "<button type='submit' name='" + _scoped(name) + "' value='1'>" + IotsaBaseModule::htmlEncode(label.text) + "</button>";
  html += " <label><input type='checkbox' name='" + _scoped(name) + ".sure' value='1'> I am sure</label>";
  if (_forms.empty()) {
    _out() += "<form method='post' class='button'>" + _formHidden + html + "</form>\n";
  } else {
    _markEditable(true, false);
    _out() += html + "\n";
  }
}

void IotsaWebContext::showCell(const IotsaWebLabel& label, const String& value, int flags) {
  if (_headerRow) {
    _out() += "<th>" + IotsaBaseModule::htmlEncode(label.text) + "</th>";
  } else if (flags & Secret) {
    _out() += String("<td>") + (value.length() ? "(set)" : "") + "</td>";
  } else {
    _out() += "<td>" + IotsaBaseModule::htmlEncode(value) + "</td>";
  }
}

String IotsaWebContext::takeOutput() {
  // Why some fields of this part are plain text, once, at the top.
  String rv;
  if (_plainForMode) rv += "<p>Some of these can only be changed in configuration mode, see <a href='/runmode'>/runmode</a>.</p>\n";
  if (_plainForRight) rv += "<p>Some of these can't be changed with the rights you have.</p>\n";
  rv += _output;
  _output = "";
  _plainForMode = false;
  _plainForRight = false;
  return rv;
}

// Sent in pieces, not concatenated: a page with a long list (a door's RFID
// cards) would otherwise be in memory twice (cwi-dis/iotsaDoorOpener#1).
void IotsaWebContext::sendPage(const String& title, const String& everyday, const String& settings) {
  String head = "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n";
  head += "<meta name='viewport' content='width=device-width, initial-scale=1'>\n";
  head += "<title>" + IotsaBaseModule::htmlEncode(title) + "</title>\n";
  head += "</head><body>\n";
  head += "<h1>" + IotsaBaseModule::htmlEncode(title) + "</h1>\n";
  if (failed()) {
    head += "<p class='error'><strong>" + IotsaBaseModule::htmlEncode(errorMessage()) + "</strong></p>\n";
  }
  for (auto& f : _feedback) {
    head += "<p>" + IotsaBaseModule::htmlEncode(f.text) + "</p>\n";
  }
#ifdef CONTENT_LENGTH_UNKNOWN
  _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
#endif
  _server->send(200, "text/html", head);
  // An empty chunk would end the response.
  if (everyday.length() > 0) _server->sendContent(everyday);
  if (settings.length() > 0) {
    _server->sendContent("<h2>Settings</h2>\n");
    _server->sendContent(settings);
  }
  _server->sendContent("<p><a href='/'>Home</a></p>\n</body></html>\n");
}

void IotsaWebContext::sendText() {
  if (failed()) {
    _server->send(errorStatus(), "text/plain", errorMessage() + "\n");
    return;
  }
  String text;
  for (auto& f : _feedback) {
    text += f.text + "\n";
  }
  if (text.length() == 0) text = "OK\n";
  _server->send(200, "text/plain", text);
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
  if (ctx.argPeek("version", v) && v != String(_currentVersion())) {
    ctx.error("This page was out of date, nothing changed. Reload it and try again.");
    return;
  }
  if (_ops & Remove) {
    for (int i = 0; i < size(); i++) {
      IotsaWebContext::Scope s(ctx, key(i));
      IotsaWebContext::Change change(ctx, "remove", _persistence);
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
    IotsaWebContext::Change change(ctx, "add", _persistence);
    if (change) {
      withExample([&](IotsaModObject& obj) {
        obj.formHandler_args(ctx);
        // A refused field fails the whole add.
        if (change && add(obj, ctx)) {
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
  bool removable = (_ops & Remove) && ctx.editable(_persistence);
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
        ctx.showButton("remove", "Remove", _persistence);
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
      if (removable) ctx.showButton("remove", "Remove", _persistence);
    }
  }
  if ((_ops & Add) && ctx.editable(_persistence)) {
    IotsaWebContext::Scope s(ctx, "new");
    IotsaWebContext::Form form(ctx, "Add", "Add", "add");
    withExample([&](IotsaModObject& obj) { obj.formHandler_fields(ctx); });
  }
}

#endif // IOTSA_WITH_WEB
