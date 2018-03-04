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
  bool needsAuthentication(const char *obj, IotsaApiOperation verb);
private:
  String obj;
  IotsaCapabilityObjectScope scopes[3];
  IotsaCapability *next;
};

class IotsaCapabilityMod : public IotsaAuthMod, public IotsaApiProvider {
public:
  IotsaCapabilityMod(IotsaApplication &_app, IotsaAuthMod &_chain);
  void setup();
  void serverSetup();
  void loop();
  String info();
  bool needsAuthentication(const char *obj, IotsaApiOperation verb);
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
protected:
  void loadCapabilitiesFromRequest();
  void configLoad();
  void configSave();
  void handler();
  IotsaCapability *capabilities;
  IotsaApi api;
  IotsaAuthMod &chain;
  String trustedIssuer;
  String issuerKey;
};

#endif
