#ifndef _WAPPOTA_H_
#define _WAPPOTA_H_
#include "iotsa.h"

class IotsaOtaMod : public IotsaMod {
public:
  IotsaOtaMod(IotsaApplication &_app) : IotsaMod(_app) {}
	void setup();
	void serverSetup();
	void loop();
	String info();
};

#endif
