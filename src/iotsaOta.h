#ifndef _WAPPOTA_H_
#define _WAPPOTA_H_
#include "Wapp.h"

class WappOtaMod : public WappMod {
public:
  WappOtaMod(Wapplication &_app) : WappMod(_app) {}
	void setup();
	void serverSetup();
	void loop();
	String info();
};

#endif
