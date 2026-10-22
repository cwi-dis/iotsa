#ifndef _IOTSACAPABILITIES_H_
#define _IOTSACAPABILITIES_H_
#include "iotsa.h"
#include "iotsaApi.h"

typedef enum IotsaCapabilityObjectScope {
  IOTSA_SCOPE_NONE,
  IOTSA_SCOPE_SELF,
  IOTSA_SCOPE_FULL,
  IOTSA_SCOPE_CHILD
} IotsaCapabilityObjectScope;

class IotsaCapability {
  friend class IotsaCapabilityMod;
public:
  IotsaCapability(String _obj, IotsaCapabilityObjectScope _get, IotsaCapabilityObjectScope _put, IotsaCapabilityObjectScope _post)
  : obj(_obj),
    next(NULL)
  { scopes[0] = _get; scopes[1] = _put; scopes[2] = _post; }
  bool allows(const char *obj, IotsaApiOperation verb);
private:
  String obj;
  IotsaCapabilityObjectScope scopes[3];
  IotsaCapability *next;
};

class IotsaCapabilityMod : public IotsaAuthMod {
public:
  IotsaCapabilityMod(IotsaApplication &_app, IotsaAuthenticationProvider &_chain);
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
  IotsaAccess allows(IotsaContext& ctx, const char *right) override;
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
protected:
  void loadCapabilitiesFromRequest(IotsaContext& ctx);
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Capability Authority"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
  IotsaCapability *capabilities;
  IotsaAuthenticationProvider &chain;
  String trustedIssuer;
  String issuerKey;
};

#endif
