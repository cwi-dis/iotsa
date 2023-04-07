#ifndef _IOTSAHPSAPI_H_
#define _IOTSAHPSAPI_H_
#include "iotsa.h"
#include <list>


class IotsaHpsApiService : public IotsaApiServiceProvider {
public:
  IotsaHpsApiService(IotsaApiProvider* _provider, IotsaApplication &_app, IotsaAuthenticationProvider* _auth);
  void setup(const char* path, bool get=false, bool put=false, bool post=false) override;
private:
  IotsaAuthenticationProvider* auth;
public:
  IotsaApiProvider* provider; 
  const char *provider_path=nullptr;
  bool provider_get=false;
  bool provider_put=false;
  bool provider_post=false;
  static std::list<IotsaHpsApiService*> all;
};

#endif