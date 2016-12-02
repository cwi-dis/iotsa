#ifndef _IOTSAFILES_H_
#define _IOTSAFILES_H_
#include "iotsa.h"

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
};

#endif
