#include "iotsaApi.h"
#include <algorithm>

uint32_t IotsaCollection::_currentVersion() {
  if (_version == 0) {
#ifdef ESP32
    _version = esp_random() | 1;
#else
    _version = RANDOM_REG32 | 1;
#endif
  }
  return _version;
}

int IotsaCollection::_find(const String& k) {
  for (int i = 0; i < size(); i++) {
    if (key(i) == k) return i;
  }
  return -1;
}

//
// The API half (cwi-dis/iotsa#309).
//

void IotsaCollection::getHandler(IotsaApiContext& ctx) {
  JsonArray list = ctx.reply()[_name].to<JsonArray>();
  for (int i = 0; i < size(); i++) {
    IotsaApiContext::Scope s(ctx, _scope(key(i)), JsonObject(), list.add<JsonObject>());
    at(i)->getHandler(ctx);
  }
}

void IotsaCollection::putHandler(IotsaApiContext& ctx) {
  JsonVariant v = ctx.request()[_name];
  if (v.isNull()) return;
  if (v.is<JsonArray>()) {
    _replaceAll(ctx, v.as<JsonArray>());
  } else if (v.is<JsonObject>()) {
    _putByKey(ctx, v.as<JsonObject>());
  } else {
    ctx.used(_name);
    ctx.error(400, String("wrong type: ") + _name);
  }
}

void IotsaCollection::postHandler(IotsaApiContext& ctx) {
  JsonVariant v = ctx.request()[_name];
  if (v.isNull()) return;
  if (!v.is<JsonObject>()) {
    ctx.used(_name);
    ctx.error(400, String("wrong type: ") + _name);
    return;
  }
  IotsaApiContext::Change change(ctx, _name, _persistence);
  if (!change) return;
  if (!(_ops & Add)) {
    ctx.error(405, String("can't add to ") + _name);
    return;
  }
  withExample([&](IotsaApiModObject& obj) {
    {
      IotsaApiContext::Scope s(ctx, _scope("new"), v.as<JsonObject>(), JsonObject());
      obj.putHandler(ctx);
    }
    // A refused field fails the whole add.
    if (change && add(obj, ctx)) {
      _version++;
      change.done();
    }
  });
}

// An array: the new list. Nothing changes unless every element is accepted.
void IotsaCollection::_replaceAll(IotsaApiContext& ctx, JsonArray list) {
  IotsaApiContext::Change change(ctx, _name, _persistence);
  if (!change) return;
  if ((_ops & (Add | Remove)) != (Add | Remove)) {
    ctx.error(405, String("can't replace ") + _name);
    return;
  }
  const char *field = keyField();
  std::vector<String> seen;
  replaceBegin();
  int i = 0;
  for (JsonVariant v : list) {
    if (!v.is<JsonObject>()) {
      ctx.error(400, String(_name) + ": every element must be an object");
      break;
    }
    JsonObject element = v.as<JsonObject>();
    String k(i);
    int from = -1;
    if (field) {
      k = element[field].as<String>();
      if (k == "") {
        ctx.error(400, String(_name) + ": an element without " + field);
        break;
      }
      if (std::find(seen.begin(), seen.end(), k) != seen.end()) {
        ctx.error(400, String(_name) + ": " + k + " twice");
        break;
      }
      seen.push_back(k);
      from = _find(k);
    }
    IotsaApiModObject *obj = replaceAdd(from);
    {
      IotsaApiContext::Scope s(ctx, _scope(k), element, JsonObject());
      // A copy has its name already.
      if (from >= 0) ctx.used(field);
      obj->putHandler(ctx);
    }
    if (ctx.failed()) break;
    if (from < 0 && !replaceAccept(*obj, ctx)) break;
    i++;
  }
  bool ok = (bool)change;
  replaceEnd(ok);
  if (ok) {
    _version++;
    change.done();
  }
}

// An object keyed by key: change or add those elements, null removes. The
// removals come last, from the highest index down, so an index key means the
// same element during the whole request.
void IotsaCollection::_putByKey(IotsaApiContext& ctx, JsonObject elements) {
  IotsaApiContext::Change change(ctx, _name, _persistence);
  if (!change) return;
  const char *field = keyField();
  std::vector<int> removals;
  for (JsonPair kv : elements) {
    String k = kv.key().c_str();
    JsonVariant v = kv.value();
    int i = _find(k);
    if (v.isNull()) {
      if (i < 0) {
        ctx.error(404, String("no such ") + _scope(k));
        break;
      }
      if (!(_ops & Remove)) {
        ctx.error(405, String("can't remove from ") + _name);
        break;
      }
      removals.push_back(i);
      continue;
    }
    if (!v.is<JsonObject>()) {
      ctx.error(400, String("wrong type: ") + _scope(k));
      break;
    }
    JsonObject element = v.as<JsonObject>();
    if (i >= 0) {
      IotsaApiContext::Scope s(ctx, _scope(k), element, JsonObject());
      at(i)->putHandler(ctx);
    } else if (field && (_ops & Add)) {
      // A new element, named by its key.
      element[field] = k;
      withExample([&](IotsaApiModObject& obj) {
        {
          IotsaApiContext::Scope s(ctx, _scope(k), element, JsonObject());
          obj.putHandler(ctx);
        }
        if (!ctx.failed()) add(obj, ctx);
      });
    } else {
      ctx.error(404, String("no such ") + _scope(k));
    }
    if (ctx.failed()) break;
  }
  if (!change) return;
  std::sort(removals.begin(), removals.end(), std::greater<int>());
  for (int i : removals) remove(i);
  _version++;
  change.done();
}

#ifdef IOTSA_WITH_WEB
//
// The web half (cwi-dis/iotsa#183).
//

void IotsaCollection::formHandler_args(IotsaWebContext& ctx) {
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
      withExample([&](IotsaApiModObject& obj) {
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

void IotsaCollection::formHandler_fields(IotsaWebContext& ctx) {
  IotsaWebContext::FormHidden version(ctx, "version", String(_currentVersion()));
  bool removable = (_ops & Remove) && ctx.editable(_persistence);
  if (_presentation == Table) {
    ctx << "<table>\n<tr>";
    ctx._headerRow = true;
    withExample([&](IotsaApiModObject& obj) { obj.formHandler_row(ctx); });
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
    withExample([&](IotsaApiModObject& obj) { obj.formHandler_fields(ctx); });
  }
}
#endif // IOTSA_WITH_WEB
