#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"

#include <string>

IotsaConfigFileLoad::IotsaConfigFileLoad(String filename) {
  IotsaConfigFileLoad(filename.c_str());
}

IotsaConfigFileLoad::IotsaConfigFileLoad(const char *filename) {
  fp = IOTSA_FS.open(filename, "r");
  if (!fp) {
    IotsaSerial.printf("IotsaConfigFileLoad: %s not found\n", filename);
  }
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

void IotsaConfigFileLoad::get(String name, uint32_t &value, uint32_t def) {
  String sValue;
  String sDef = String(def);
  get(name, sValue, sDef);
  value = (uint32_t)sValue.toInt();
}

void IotsaConfigFileLoad::get(String name, uint16_t &value, uint16_t def) {
  int iDef = (int)def;
  int iValue;
  get(name, iValue, iDef);
  value = (uint16_t)iValue;
}

void IotsaConfigFileLoad::get(String name, uint8_t &value, uint8_t def) {
  int iDef = (int)def;
  int iValue;
  get(name, iValue, iDef);
  value = (uint8_t)iValue;
}

void IotsaConfigFileLoad::get(String name, bool &value, bool def) {
  int iDef = (int)def;
  int iValue;
  get(name, iValue, iDef);
  value = (bool)iValue;
}

void IotsaConfigFileLoad::get(String name, float &value, float def) {
  String sValue;
  String sDef = String(def);
  get(name, sValue, sDef);
  value = sValue.toFloat();
}

void IotsaConfigFileLoad::get(String name, String &value, const String &def) {
  get(name, value, def.c_str());
}

void IotsaConfigFileLoad::get(String name, std::string &value, const std::string &def) {
  String sValue;
  get(name, sValue, def.c_str());
  value = sValue.c_str();
}

void IotsaConfigFileLoad::get(String name, String &value, const char *def) {
  fp.seek(0, SeekSet);
  while (fp.available()) {
    String configName = fp.readStringUntil('=');
    String configValue = fp.readStringUntil('\n');
#if 0
	// Enabling this is a security risk, it allows obtaining passwords and such with physical access.
    IFDEBUG IotsaSerial.print("cfload: found value ");
    IFDEBUG IotsaSerial.println(configValue);
#endif
    if (configName == name) {
      IFDEBUG IotsaSerial.print("cfload: found ");
      IFDEBUG IotsaSerial.println(name);
      value = configValue;
      return;
    }
  }
  IFDEBUG IotsaSerial.print("cfload: did not find ");
  IFDEBUG IotsaSerial.println(name);
  value = String(def);
}

IotsaConfigFileSave::IotsaConfigFileSave(String filename) {
  IotsaConfigFileSave(filename.c_str());
}

IotsaConfigFileSave::IotsaConfigFileSave(const char *filename) {
#ifndef IOTSA_WITH_LEGACY_SPIFFS_xxxjack
  // FS.open() needs extra create argument true to create intermediate directories
  fp = IOTSA_FS.open(filename, "w", true);
#else
  fp = IOTSA_FS.open(filename, "w");
#endif
  if (!fp) {
    IotsaSerial.printf("IotsaConfigFileSave: %s not created\n", filename);
  }
}

IotsaConfigFileSave::~IotsaConfigFileSave() {
  fp.close();
}

void IotsaConfigFileSave::put(String name, int value) {
  String sValue = String(value);
  put(name, sValue);
}

void IotsaConfigFileSave::put(String name, float value) {
  String sValue = String(value, 6);
  put(name, sValue);
}

void IotsaConfigFileSave::put(String name, const String &value) {
  fp.print(name);
  fp.print('=');
  fp.print(value);
  fp.print('\n');
}

void IotsaConfigFileSave::put(String name, const std::string &value) {
  fp.print(name);
  fp.print('=');
  fp.print(value.c_str());
  fp.print('\n');
}

bool iotsaConfigFileExists(String filename) {
  return IOTSA_FS.exists(filename);
}

bool iotsaConfigFileLoadBinary(String filename, uint8_t **dataP, size_t *dataLenP) {
  File fp = IOTSA_FS.open(filename, "r");
  if (!fp) return false;
  size_t size = fp.size();
  if (size == 0) {
    IFDEBUG IotsaSerial.println("iotsaConfigFileLoadBinary empty file");
    fp.close();
    return false;
  }
  uint8_t *buf = (uint8_t *)malloc(size);
  if (buf == NULL) {
    IFDEBUG IotsaSerial.println("iotsaConfigFileLoadBinary malloc failed");
    fp.close();
    return false;
  }
  if (fp.readBytes((char *)buf, size) != size) {
    IFDEBUG IotsaSerial.println("iotsaConfigFileLoadBinary read wrong size");
    fp.close();
    return false;
  }
  fp.close();
  *dataP = buf;
  *dataLenP = size;
  return true;
}

void iotsaConfigFileSaveBinary(String filename, const uint8_t *data, size_t dataLen) {
  File fp = IOTSA_FS.open(filename, "w");
  if (fp.write(data, dataLen) != dataLen) {
    IFDEBUG IotsaSerial.println("iotsaConfigFileSaveBinary write wrong size");    
  }
  fp.close();
}
