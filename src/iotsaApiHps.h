#ifndef _IOTSAAPIHPS_H_
#define _IOTSAAPIHPS_H_
#include "iotsa.h"
#include <list>

#ifdef IOTSA_HAS_HPSSERVER

// HPS = HTTP Proxy Service, a Bluetooth SIG-adopted GATT service (not an
// iotsa-invented acronym): UUID 0x1823, with characteristics 0x2AB6-0x2ABB
// below. Lets a BLE client tunnel HTTP-shaped requests (URI, headers, status
// code, body, control point, security) through this device acting as a
// gateway -- exactly how the `iotsa` CLI's `--protocol hps` reaches BLE-only
// devices like `control`. Spec:
// https://www.bluetooth.com/specifications/specs/http-proxy-service-1-0/
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