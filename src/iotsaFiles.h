#ifndef _IOTSAFILES_H_
#define _IOTSAFILES_H_
#include "iotsa.h"

#ifdef IOTSA_WITH_WEB
class IotsaFilesMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup();
  void serverSetup();
  void loop();
  String info();
  virtual bool accessAllowed(String &path);	// Return true if allowed, default only for /data/*
private:
  void listHandler();
  void notFoundHandler();
  void _listDir(String& message, const char *name);
};
#elif IOTSA_WITH_PLACEHOLDERS
class IotsaFilesMod : public IotsaMod {
public:
  using IotsaMod::IotsaMod;
  void setup() {}
  void serverSetup() {}
  void loop() {}
};
#endif // IOTSA_WITH_WEB || IOTSA_WITH_PLACEHOLDERS
#endif
