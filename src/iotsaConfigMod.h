#ifndef _IOTSACONFIGMOD_H_
#define _IOTSACONFIGMOD_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaConfigMod : public IotsaModule, public IotsaSingletonModule<IotsaConfigMod> {
public:
  IotsaConfigMod(IotsaApplication &_app)
  : IotsaModule(_app, true)
#ifdef IOTSA_WITH_HTTPS
  , newCertificate(NULL),
  newCertificateLength(0),
  newKey(NULL),
  newKeyLength(0)
#endif // IOTSA_WITH_HTTPS
  {
    claimSingleton(this);
  }
	void setup() override;
	void lateSetup() override;
	void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
private:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  String webTitle() override { return "Iotsa configuration"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
#endif
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* newCertificate;
  size_t newCertificateLength;
  const uint8_t* newKey;
  size_t newKeyLength;
#endif // IOTSA_WITH_HTTPS
};

#endif
