#ifndef _IOTSASIMPLE_H_
#define _IOTSASIMPLE_H_
#include "iotsa.h"

typedef void (*handlerfunc)();
typedef String (*infofunc)();

class IotsaSimpleMod : public IotsaMod {
  public:
	IotsaSimpleMod(IotsaApplication &_app, const char *_url, handlerfunc _hfun, infofunc _ifun=NULL)
	:	IotsaMod(_app),
   		url(_url),
   		hfun(_hfun),
   		ifun(_ifun)
	{}
	void setup() override;
	void serverSetup() override;
	void loop() override;
#ifdef IOTSA_WITH_WEB
	String info() override;
#endif
  protected:
  	const char *url;
  	handlerfunc hfun;
  	infofunc ifun;
};

#endif
