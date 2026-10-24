#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"

#include <string>

// A value is one line in the file, so newlines are escaped (and the escape
// character itself): \\ \n \r. On reading, a backslash before any other
// character is kept as it is (cwi-dis/iotsa#197).
static String _escape(const char *value) {
  String out;
  for (const char *p = value; *p; p++) {
    if (*p == '\\') out += "\\\\";
    else if (*p == '\n') out += "\\n";
    else if (*p == '\r') out += "\\r";
    else out += *p;
  }
  return out;
}

static String _unescape(const String& value) {
  if (value.indexOf('\\') < 0) return value;
  String out;
  for (unsigned int i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '\\' && i + 1 < value.length()) {
      char n = value[i+1];
      if (n == '\\') { out += '\\'; i++; continue; }
      if (n == 'n') { out += '\n'; i++; continue; }
      if (n == 'r') { out += '\r'; i++; continue; }
    }
    out += c;
  }
  return out;
}

IotsaConfigFileLoad::IotsaConfigFileLoad(String filename)
: IotsaConfigFileLoad(filename.c_str())
{
}

IotsaConfigFileLoad::IotsaConfigFileLoad(const char *filename) {
  fp = IOTSA_FS.open(filename, "r");
  if (!fp) {
    IotsaSerial.printf("IotsaConfigFileLoad: %s: file not found\n", filename);
  }
}

IotsaConfigFileLoad::~IotsaConfigFileLoad() {
  fp.close();
}

bool IotsaConfigFileLoad::get(String name, int &value, int def) {
  String sValue;
  String sDef = String(def);
  bool found = get(name, sValue, sDef);
  value = sValue.toInt();
  return found;
}

bool IotsaConfigFileLoad::get(String name, uint32_t &value, uint32_t def) {
  String sValue;
  String sDef = String(def);
  bool found = get(name, sValue, sDef);
  value = (uint32_t)sValue.toInt();
  return found;
}

bool IotsaConfigFileLoad::get(String name, uint16_t &value, uint16_t def) {
  int iDef = (int)def;
  int iValue;
  bool found = get(name, iValue, iDef);
  value = (uint16_t)iValue;
  return found;
}

bool IotsaConfigFileLoad::get(String name, uint8_t &value, uint8_t def) {
  int iDef = (int)def;
  int iValue;
  bool found = get(name, iValue, iDef);
  value = (uint8_t)iValue;
  return found;
}

bool IotsaConfigFileLoad::get(String name, bool &value, bool def) {
  int iDef = (int)def;
  int iValue;
  bool found = get(name, iValue, iDef);
  value = (bool)iValue;
  return found;
}

bool IotsaConfigFileLoad::get(String name, float &value, float def) {
  String sValue;
  String sDef = String(def);
  bool found = get(name, sValue, sDef);
  value = sValue.toFloat();
  return found;
}

bool IotsaConfigFileLoad::get(String name, String &value, const String &def) {
  return get(name, value, def.c_str());
}

bool IotsaConfigFileLoad::get(String name, std::string &value, const std::string &def) {
  String sValue;
  bool found = get(name, sValue, def.c_str());
  value = sValue.c_str();
  return found;
}

bool IotsaConfigFileLoad::get(String name, String &value, const char *def) {
  fp.seek(0, SeekSet);
  while (fp.available()) {
    String configName = fp.readStringUntil('=');
    String configValue = fp.readStringUntil('\n');
    if (configName == name) {
      IFDEBUG IotsaSerial.print("cfload: found ");
      IFDEBUG IotsaSerial.println(name);
      value = _unescape(configValue);
      return true;
    }
  }
  IFDEBUG IotsaSerial.print("cfload: did not find ");
  IFDEBUG IotsaSerial.println(name);
  value = String(def);
  return false;
}

IotsaConfigFileSave::IotsaConfigFileSave(String filename)
: IotsaConfigFileSave(filename.c_str())
{
}

IotsaConfigFileSave::IotsaConfigFileSave(const char *filename) {
#ifndef IOTSA_FS_OPEN_2_ARGS
  // FS.open() needs extra create argument true to create intermediate directories
  fp = IOTSA_FS.open(filename, "w", true);
#else
  fp = IOTSA_FS.open(filename, "w");
#endif
  if (!fp) {
    IotsaSerial.printf("IotsaConfigFileSave: %s not created\n", filename);
  } else {
    IFDEBUG IotsaSerial.printf("IotsaConfigFileSave: created %s\n", filename);
  }
}

IotsaConfigFileSave::~IotsaConfigFileSave() {
  fp.close();
  IFDEBUG IotsaSerial.printf("IotsaConfigFileSave: closed\n");
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
  fp.print(_escape(value.c_str()));
  fp.print('\n');
}

void IotsaConfigFileSave::put(String name, const std::string &value) {
  fp.print(name);
  fp.print('=');
  fp.print(_escape(value.c_str()));
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
