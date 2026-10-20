#ifndef _IOTSASTATICAUTHMOD_H_
#define _IOTSASTATICAUTHMOD_H_

#include "iotsa.h"
#include "iotsaApi.h"

//
// An authenticator with one fixed username and password, "admin" and "admin".
// See iotsaStaticAuthMod.cpp.
//
class IotsaStaticAuthMod : public IotsaAuthMod {
public:
  using IotsaAuthMod::IotsaAuthMod;
  void setup() override;
  void loop() override;
  bool allows(const char *right=NULL) override;
  bool allows(const char *obj, IotsaApiOperation verb) override;
};

#endif
