#ifndef _WAPPHELLO_H_
#define _WAPPHELLO_H_
#include "iotsa.h"

class IotsaHelloMod : public IotsaMod {
public:
  IotsaHelloMod(IotsaApplication &_app) : IotsaMod(_app) {}
	void setup();
	void serverSetup();
	void loop();
  String info();
private:
  void handler();
};

#endif
