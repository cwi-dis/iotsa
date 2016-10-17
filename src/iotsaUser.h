#ifndef _IOTSAUSER_H_
#define _IOTSAUSER_H_
#include "iotsa.h"

class IotsaUserMod : public IotsaAuthMod {
public:
  IotsaUserMod(IotsaApplication &_app, const char *_username="admin", const char *_password="");
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool needsAuthentication();
protected:
  void configLoad();
  void configSave();
  void handler();
  String username;
  String password;
};

#endif
