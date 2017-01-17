#include "iotsa.h"
#include "iotsaConfigFile.h"

IotsaConfigFileLoad::IotsaConfigFileLoad(String filename) {
  fp = SPIFFS.open(filename, "r");
}

IotsaConfigFileLoad::IotsaConfigFileLoad(const char *filename) {
  fp = SPIFFS.open(filename, "r");
}

IotsaConfigFileLoad::~IotsaConfigFileLoad() {
  fp.close();
}

void IotsaConfigFileLoad::get(String name, int &value, int def) {
  String sValue;
  String sDef = String(def);
  get(name, sValue, sDef);
  value = sValue.toInt();
}

void IotsaConfigFileLoad::get(String name, String &value, const String &def) {
  get(name, value, def.c_str());
}

void IotsaConfigFileLoad::get(String name, String &value, const char *def) {
  fp.seek(0, SeekSet);
  IFDEBUG IotsaSerial.print("cfload: look for ");
  IFDEBUG IotsaSerial.println(name);
  while (fp.available()) {
    String configName = fp.readStringUntil('=');
    String configValue = fp.readStringUntil('\n');
    IFDEBUG IotsaSerial.print("cfload: found name ");
    IFDEBUG IotsaSerial.println(configName);
    IFDEBUG IotsaSerial.print("cfload: found value ");
    IFDEBUG IotsaSerial.println(configValue);
    if (configName == name) {
      value = configValue;
      return;
    }
  }
  value = String(def);
}

IotsaConfigFileSave::IotsaConfigFileSave(String filename) {
  fp = SPIFFS.open(filename, "w");
}

IotsaConfigFileSave::IotsaConfigFileSave(const char *filename) {
  fp = SPIFFS.open(filename, "w");
}

IotsaConfigFileSave::~IotsaConfigFileSave() {
  fp.close();
}

void IotsaConfigFileSave::put(String name, int value) {
  String sValue = String(value);
  put(name, sValue);
}

void IotsaConfigFileSave::put(String name, const String &value) {
  fp.print(name);
  fp.print('=');
  fp.print(value);
  fp.print('\n');
}

