#ifndef _IOTSAAPIHPS_H_
#define _IOTSAAPIHPS_H_
#include "iotsa.h"
#include <list>

#ifdef IOTSA_HAS_HPSSERVER

class IotsaHpsServiceMod;

class IotsaApiServiceHps : public IotsaApiServiceProvider {
public:
  typedef const char * UUIDstring;
  IotsaApiServiceHps(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth, IotsaApiServiceProvider* _next=nullptr);
  void setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true) override;
  static void ensureServiceMod(IotsaApplication &app);
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
};
#endif // IOTSA_HAS_HPSSERVER
#endif