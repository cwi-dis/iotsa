#ifndef _WAPPHELLO_H_
#define _WAPPHELLO_H_
#include "Wapp.h"

class WappHelloMod : public WappMod {
public:
  WappHelloMod(Wapplication &_app) : WappMod(_app) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void handler();
};

#endif
