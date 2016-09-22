#ifndef _WAPPFILES_H_
#define _WAPPFILES_H_
#include "Wapp.h"

class WappFilesMod : public WappMod {
public:
  WappFilesMod(Wapplication &_app) : WappMod(_app) {}
  void setup();
  void serverSetup();
  void loop();
  String info();
private:
  void listHandler();
  void uploadHandler();
  void notFoundHandler();
  void uploadOkHandler();
  void uploadFormHandler();
};

#endif
