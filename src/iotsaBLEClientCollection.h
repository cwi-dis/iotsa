#ifndef _IOTSABLECLIENTCOLLECTION_H_
#define _IOTSABLECLIENTCOLLECTION_H_
#include "iotsaBLEClient.h"

#ifdef IOTSA_WITH_BLE

#include <vector>

// The owner of a set of BLE client devices that are added and removed at
// runtime, and their persistence (cwi-dis/iotsa#286). Not a module: it lives
// inside one (by default IotsaBLEClientManagerMod).
//
// IotsaBLEClientMod's registry is only an index of the devices to scan for
// and connect to. This class creates the devices (through a factory),
// registers them there, deletes them again, and saves and loads the list.
// Each device saves its own name and address under "device<N>" (its
// configLoad()/configSave()), so a factory can return any subclass of
// IotsaBLEClientDevice.
class IotsaBLEClientCollection {
public:
  typedef std::function<IotsaBLEClientDevice*(const std::string& name)> Factory;
  typedef std::vector<IotsaBLEClientDevice*>::iterator iterator;

  IotsaBLEClientCollection(IotsaBLEClientMod& _owner) : owner(_owner) {}
  ~IotsaBLEClientCollection();
  IotsaBLEClientCollection(const IotsaBLEClientCollection&) = delete;
  IotsaBLEClientCollection& operator=(const IotsaBLEClientCollection&) = delete;
  // Default: a plain IotsaRunmodeBLEClient, so every device can do the
  // generic runmode commands (identify, reboot, ...).
  void setFactory(Factory _factory) { factory = _factory; }
  // Creates and registers a device. Returns nullptr if the name is empty or
  // already known to the registry (in this collection or elsewhere).
  IotsaBLEClientDevice* add(const std::string& name);
  // Deletes a device of this collection, by its current name. Returns false
  // if there is none.
  bool remove(const std::string& name);
  IotsaBLEClientDevice* find(const std::string& name);
  int size() { return (int)items.size(); }
  iterator begin() { return items.begin(); }
  iterator end() { return items.end(); }
  // Keys: "n_device", then "device<N>.*". With a non-empty f_name, all keys
  // get an "<f_name>." prefix.
  void configLoad(IotsaConfigFileLoad& cf, const String& f_name);
  void configSave(IotsaConfigFileSave& cf, const String& f_name);
protected:
  IotsaBLEClientDevice* _create(const std::string& name);
  IotsaBLEClientMod& owner;
  Factory factory;
  std::vector<IotsaBLEClientDevice*> items;
};

#endif // IOTSA_WITH_BLE
#endif
