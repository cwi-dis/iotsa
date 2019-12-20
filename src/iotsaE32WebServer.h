#ifndef _IOTSAWEBSERVER_H_
#define _IOTSAWEBSERVER_H_
#include "iotsaBuildOptions.h"

#include <ESPWebServer.hpp>
typedef ESPWebServer IotsaWebServer;

class IotsaApplication;

class IotsaWebServerMixin {
friend class IotsaApplication;
public:
  IotsaWebServerMixin(IotsaApplication* _app);
#ifdef IOTSA_WITH_HTTP_OR_HTTPS
  IotsaWebServer *server;
protected:
  IotsaApplication* app;
  void webServerSetup();
  void webServerLoop();
  void webServerNotFoundHandler();
#endif
#ifdef IOTSA_WITH_WEB
  void webServerRootHandler();
#endif
};

#endif
