#ifndef _IOTSACONFIGFILE_H_
#define _IOTSACONFIGFILE_H_

#include <FS.h>

class WapConfigFileLoad {
public:
  WapConfigFileLoad(String filename);
  WapConfigFileLoad(const char *filename);
  ~WapConfigFileLoad();
  void get(String name, int &value, int def);
  void get(String name, String &value, const char *def);
protected:
  File fp;
};

class WapConfigFileSave {
public:
  WapConfigFileSave(String filename);
  WapConfigFileSave(const char *filename);
  ~WapConfigFileSave();
  void put(String name, int value);
  void put(String name, String &value);
protected:
  File fp;
};
#endif
