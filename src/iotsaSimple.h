#ifndef _IOTSASIMPLE_H_
#define _IOTSASIMPLE_H_
#include "iotsa.h"

typedef void (*handlerfunc)();
typedef String (*infofunc)();

// A web-server-extension module -- HTTP is all this is, not one of several
// transports for a REST/CoAP/HPS API, so it reaches the shared server via
// app.server rather than an IotsaApiServiceWeb link (which exists to let an API
// also have a page) -- see cwi-dis/iotsa#211.
class IotsaSimpleMod : public IotsaBaseModule {
  public:
	IotsaSimpleMod(IotsaApplication &_app, const char *_url, handlerfunc _hfun, infofunc _ifun=NULL)
	:	IotsaBaseModule(_app),
   		url(_url),
   		hfun(_hfun),
   		ifun(_ifun)
	{}
	void setup() override;
	void lateSetup() override;
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
