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
  if (!_authChecked) {
    _authChecked = true;
    // Both send their own challenge when they refuse.
    if (_right) {
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

void IotsaWebContext::_changed(FieldKind kind) {
  if (kind == Live) {
    stateChanged = true;
  } else {
    configChanged = true;
  }
}

bool IotsaWebContext::password(const char *name, String& field, FieldKind kind) {
  String value;
  if (!_present(name, value)) return false;
  if (value == "" || value == field) return false;
  if (!_mayChange(kind)) return false;
  field = value;
  _changed(kind);
  return true;
}

bool IotsaWebContext::action(const char *name) {
  String value;
  if (!_present(name, value)) return false;
  return _mayChange(Live);
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
  if (kind == ConfigModeOnly) return iotsaController.inConfigurationMode();
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

void IotsaWebContext::_markEditable(bool post) {
  // An editable field makes the outermost form, which is the one that is
  // written as a <form>, editable too.
  for (auto& f : _forms) {
    f.editable = true;
    if (post) f.post = true;
  }
}

IotsaWebContext::Form::Form(IotsaWebContext& ctx, const IotsaWebLabel& submitLabel)
: _ctx(ctx)
{
  Group g;
  g.submitLabel = submitLabel.text;
  _ctx._forms.push_back(g);
}

IotsaWebContext::Form::~Form() {
  _ctx._closeForm();
}

void IotsaWebContext::_closeForm() {
  Group g = _forms.back();
  _forms.pop_back();
  if (!g.editable) {
    // Nothing to submit: no form, no button.
    _out() += g.html;
  } else if (!_forms.empty()) {
    // A form is already open: become part of it.
    _out() += "<fieldset>\n" + g.html + "</fieldset>\n";
  } else {
    _out() += String("<form method='") + (g.post ? "post" : "get") + "'>\n";
    _out() += g.html;
    _out() += "<button type='submit'>" + IotsaBaseModule::htmlEncode(g.submitLabel) + "</button>\n";
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

#endif // IOTSA_WITH_WEB
