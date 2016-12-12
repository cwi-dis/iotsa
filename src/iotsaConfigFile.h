#ifndef _IOTSACONFIGFILE_H_
#define _IOTSACONFIGFILE_H_

#include <FS.h>

class IotsaConfigFileLoad {
public:
  IotsaConfigFileLoad(String filename);
  IotsaConfigFileLoad(const char *filename);
  ~IotsaConfigFileLoad();
  void get(String name, int &value, int def);
  void get(String name, String &value, const char *def);
  void get(String name, String &value, const String &def);
protected:
  File fp;
};

class IotsaConfigFileSave {
public:
  IotsaConfigFileSave(String filename);
  IotsaConfigFileSave(const char *filename);
  ~IotsaConfigFileSave();
  void put(String name, int value);
  void put(String name, const String &value);
protected:
  File fp;
};
#endif
