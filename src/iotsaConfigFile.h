#ifndef _IOTSACONFIGFILE_H_
#define _IOTSACONFIGFILE_H_

#include <FS.h>

class IotsaConfigFileLoad {
public:
  IotsaConfigFileLoad(String filename);
  IotsaConfigFileLoad(const char *filename);
  ~IotsaConfigFileLoad();
  void get(String name, int &value, int def);
  void get(String name, uint32_t &value, uint32_t def);
  void get(String name, uint16_t &value, uint16_t def);
  void get(String name, uint8_t &value, uint8_t def);
  void get(String name, bool &value, bool def);
  void get(String name, float &value, float def);
  void get(String name, String &value, const char *def);
  void get(String name, String &value, const String &def);
  void get(String name, std::string &value, const std::string &def);
protected:
  File fp;
};

class IotsaConfigFileSave {
public:
  IotsaConfigFileSave(String filename);
  IotsaConfigFileSave(const char *filename);
  ~IotsaConfigFileSave();
  void put(String name, int value);
  inline void put(String name, uint32_t value) {put(name, (int)value); }
  inline void put(String name, uint16_t value) {put(name, (int)value); }
  inline void put(String name, uint8_t value) {put(name, (int)value); }
  inline void put(String name, bool value) {put(name, (int)value); }
  void put(String name, float value);
  void put(String name, const String &value);
  void put(String name, const std::string &value);
protected:
  File fp;
};

bool iotsaConfigFileExists(String filename);
bool iotsaConfigFileLoadBinary(String filename, uint8_t **dataP, size_t *dataLenP);
void iotsaConfigFileSaveBinary(String filename, const uint8_t *data, size_t dataLen);
#endif
