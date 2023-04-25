#ifndef _IOTSAAPIHPS_H_
#define _IOTSAAPIHPS_H_
#include "iotsa.h"
#include <list>

#ifdef IOTSA_WITH_HPS

class IotsaHpsServiceMod;

class IotsaApiServiceHps : public IotsaApiServiceProvider {
public:
  typedef const char * UUIDstring;
  IotsaApiServiceHps(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth);
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override;
private:
  IotsaAuthenticationProvider* auth;
  IotsaApiProvider* provider;
public:
  static constexpr UUIDstring serviceUUID = "1823";
  static constexpr UUIDstring urlUUID = "2AB6";
  static constexpr UUIDstring headersUUID = "2AB7";
  static constexpr UUIDstring statusUUID = "2AB8";
  static constexpr UUIDstring bodyUUID = "2AB9";
  static constexpr UUIDstring controlPointUUID = "2ABA";
  static constexpr UUIDstring securityUUID = "2ABB";
  static IotsaHpsServiceMod *_hpsMod;
};
#endif // IOTSA_WITH_HPS
#endif