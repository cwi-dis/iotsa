#include "iotsaBLEClientCollection.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaRunmodeBLEClient.h"

IotsaBLEClientCollection::~IotsaBLEClientCollection() {
  for (auto d : items) delete d;
}

IotsaBLEClientDevice* IotsaBLEClientCollection::_create(const std::string& name) {
  IotsaBLEClientDevice* dev = factory ? factory(name) : new IotsaRunmodeBLEClient(name);
  if (dev == nullptr) return nullptr;
  owner.addDevice(name, dev);
  items.push_back(dev);
  return dev;
}

IotsaBLEClientDevice* IotsaBLEClientCollection::add(const std::string& name) {
  if (name == "" || owner.getDevice(name) != nullptr) return nullptr;
  return _create(name);
}

bool IotsaBLEClientCollection::remove(const std::string& name) {
  for (auto it = items.begin(); it != items.end(); ++it) {
    if ((*it)->getName() == name) {
      IotsaBLEClientDevice* dev = *it;
      items.erase(it);
      delete dev;   // takes itself out of the registry
      return true;
    }
  }
  return false;
}

IotsaBLEClientDevice* IotsaBLEClientCollection::find(const std::string& name) {
  for (auto d : items) {
    if (d->getName() == name) return d;
  }
  return nullptr;
}

void IotsaBLEClientCollection::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
  String prefix = f_name == "" ? "" : f_name + ".";
  int n_device;
  cf.get(prefix + "n_device", n_device, 0);
  for (int i = 0; i < n_device; i++) {
    String ident = prefix + "device" + String(i);
    std::string name;
    cf.get(ident + ".name", name, "");
    if (name == "" || find(name) != nullptr) continue;
    IotsaBLEClientDevice* dev = _create(name);
    if (dev) dev->configLoad(cf, ident);
  }
}

void IotsaBLEClientCollection::configSave(IotsaConfigFileSave& cf, const String& f_name) {
  String prefix = f_name == "" ? "" : f_name + ".";
  cf.put(prefix + "n_device", (int)items.size());
  int i = 0;
  for (auto d : items) {
    d->configSave(cf, prefix + "device" + String(i++));
  }
}

#endif // IOTSA_WITH_BLE
