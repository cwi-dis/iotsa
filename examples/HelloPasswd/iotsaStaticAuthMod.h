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
  IotsaAccess allows(IotsaContext& ctx, const char *right) override;
};

#endif
