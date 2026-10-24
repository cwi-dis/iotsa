#ifndef _IOTSACOLLECTION_H_
#define _IOTSACOLLECTION_H_
#include <vector>
#include <functional>
#include "iotsa.h"
#include "iotsaContext.h"

class IotsaApiModObject;
#ifdef IOTSA_WITH_WEB
class IotsaWebContext;
#endif

//
// A collection: a list of elements of one kind in a module, with Add and
// Remove (users, tokens, forwarders, RFID cards). An interface on top of the
// module's own container: the container answers a few questions, the API
// (cwi-dis/iotsa#309) and the web page (cwi-dis/iotsa#183) are written once,
// here. The module calls the handlers from its own, the way it calls an
// object's; the elements are IotsaApiModObjects.
//
// The shape in JSON, under the collection's name (cwi-dis/iotsa#309):
// - GET: "users": [{...}, ...].
// - PUT "users": [...]: replace all, so a restore sends back what a backup
//   read. With names (keyField()) an element starts as a copy of the existing
//   one with the same name; with index keys it starts empty.
// - PUT "users": {"jack": {...}}: change jack, or add jack if there is none;
//   "jack": null removes jack (an error if there is none).
// - POST "tokens": {...}: add one element.
// An array replaces, an object merges by key, null removes: iotsa's own rule,
// borrowed from JSON Merge Patch (RFC 7386), but not that.
//
// There is no Reorder on purpose: no collection has an order that means
// something (cwi-dis/iotsa433#2), and a PUT of the whole list can reorder.
//
class IotsaCollection {
public:
  enum Ops { Add = 1, Remove = 2 };
  enum Presentation { Table, Sections };
  IotsaCollection(const char *name, int ops, Presentation presentation, IotsaContext::Persistence persistence)
  : _name(name), _ops(ops), _presentation(presentation), _persistence(persistence) {}
  virtual ~IotsaCollection() {}
  const char *name() const { return _name; }

  // ---- the container answers these ----
  virtual int size() = 0;
  virtual IotsaApiModObject* at(int i) = 0;
  // How an element is named on the page and in the API (cwi-dis/iotsa#217):
  // its name where it has one, else its index.
  virtual String key(int i) { return String(i); }
  // The element field that holds its name ("username"), or nullptr when the
  // key is the index. A keyed PUT that adds an element sets it.
  virtual const char *keyField() { return nullptr; }
  // Call fn with a new, empty element: for the table header and the add form.
  virtual void withExample(std::function<void(IotsaApiModObject&)> fn) = 0;
  // Add the filled-in example, or refuse with ctx.error().
  virtual bool add(IotsaApiModObject& obj, IotsaContext& ctx) { return false; }
  virtual bool remove(int i) { return false; }
  // Replace all, in steps, so nothing changes unless every element is
  // accepted: start a new list; add an element to it, a copy of element
  // `from` or, for -1, an empty one; check a new one as add() would; then
  // swap the new list in, or drop it.
  virtual void replaceBegin() {}
  virtual IotsaApiModObject* replaceAdd(int from) { return nullptr; }
  virtual bool replaceAccept(IotsaApiModObject& obj, IotsaContext& ctx) { return true; }
  virtual void replaceEnd(bool commit) {}

  // ---- written once, here ----
  void getHandler(IotsaApiContext& ctx);
  void putHandler(IotsaApiContext& ctx);
  void postHandler(IotsaApiContext& ctx);
#ifdef IOTSA_WITH_WEB
  void formHandler_args(IotsaWebContext& ctx);
  void formHandler_fields(IotsaWebContext& ctx);
#endif
protected:
  int _find(const String& k);
  void _replaceAll(IotsaApiContext& ctx, JsonArray list);
  void _putByKey(IotsaApiContext& ctx, JsonObject elements);
  String _scope(const String& k) const { return String(_name) + "." + k; }
  const char *_name;
  int _ops;
  Presentation _presentation;
  IotsaContext::Persistence _persistence;
  // Bumped on every change, and in every form of the collection, so a stale
  // page (a reload, a bookmark, another browser) changes nothing. Random at
  // boot, so a page from before a reboot is stale too.
  uint32_t _version = 0;
  uint32_t _currentVersion();
};

// The common case: a std::vector of elements (users, tokens). A module
// subclasses it and names it after its elements, "<Element>List"
// (IotsaUserList), overriding only what differs: key() and keyField() when
// the elements have a name, accept() to check (and complete) a new element
// before it is added.
template<typename T> class IotsaVector : public IotsaCollection {
public:
  IotsaVector(const char *name, std::vector<T>& v, int ops, Presentation presentation, IotsaContext::Persistence persistence)
  : IotsaCollection(name, ops, presentation, persistence), _v(v) {}
  int size() override { return _v.size(); }
  IotsaApiModObject* at(int i) override { return &_v[i]; }
  void withExample(std::function<void(IotsaApiModObject&)> fn) override { T example; fn(example); }
  bool add(IotsaApiModObject& obj, IotsaContext& ctx) override {
    T& t = static_cast<T&>(obj);
    if (!accept(t, ctx)) return false;
    _v.push_back(t);
    return true;
  }
  bool remove(int i) override { _v.erase(_v.begin() + i); return true; }
  void replaceBegin() override { _staged.clear(); _staged.reserve(_v.size()); }
  IotsaApiModObject* replaceAdd(int from) override {
    _staged.push_back(from >= 0 ? _v[from] : T());
    return &_staged.back();
  }
  bool replaceAccept(IotsaApiModObject& obj, IotsaContext& ctx) override { return accept(static_cast<T&>(obj), ctx); }
  void replaceEnd(bool commit) override {
    if (commit) _v.swap(_staged);
    _staged.clear();
  }
protected:
  // A new element: refuse it with ctx.error() and false. Default: accepted.
  virtual bool accept(T& t, IotsaContext& ctx) { return true; }
  std::vector<T>& _v;
  std::vector<T> _staged;
};

#endif
