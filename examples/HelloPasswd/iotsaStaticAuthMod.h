#ifndef _IOTSASTATICAUTHMOD_H_
#define _IOTSASTATICAUTHMOD_H_

#include "iotsa.h"

//
// Authentication class. Requires username/password to match before allowing changing of
// the user name to be greeted.
//
class IotsaStaticAuthMod : public IotsaAuthMod {
public:
  using IotsaAuthMod::IotsaAuthMod;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
};

#endif
