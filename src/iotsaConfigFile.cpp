#include "Wapp.h"
#include "WappConfigFile.h"

WapConfigFileLoad::WapConfigFileLoad(String filename) {
  fp = SPIFFS.open(filename, "r");
}

WapConfigFileLoad::WapConfigFileLoad(const char *filename) {
  fp = SPIFFS.open(filename, "r");
}

WapConfigFileLoad::~WapConfigFileLoad() {
  fp.close();
}

void WapConfigFileLoad::get(String name, int &value, int def) {
  String sValue;
  String sDef = String(def);
  get(name, sValue, sDef.c_str());
  value = sValue.toInt();
}

void WapConfigFileLoad::get(String name, String &value, const char *def) {
  fp.seek(0, SeekSet);
  IFDEBUG Serial.print("cfload: look for ");
  IFDEBUG Serial.println(name);
  while (fp.available()) {
    String configName = fp.readStringUntil('=');
    String configValue = fp.readStringUntil('\n');
    IFDEBUG Serial.print("cfload: found name ");
    IFDEBUG Serial.println(configName);
    IFDEBUG Serial.print("cfload: found value ");
    IFDEBUG Serial.println(configValue);
    if (configName == name) {
      value = configValue;
      return;
    }
  }
  value = String(def);
}

WapConfigFileSave::WapConfigFileSave(String filename) {
  fp = SPIFFS.open(filename, "w");
}

WapConfigFileSave::WapConfigFileSave(const char *filename) {
  fp = SPIFFS.open(filename, "w");
}

WapConfigFileSave::~WapConfigFileSave() {
  fp.close();
}

void WapConfigFileSave::put(String name, int value) {
  String sValue = String(value);
  put(name, sValue);
}

void WapConfigFileSave::put(String name, String &value) {
  fp.print(name);
  fp.print('=');
  fp.print(value);
  fp.print('\n');
}

