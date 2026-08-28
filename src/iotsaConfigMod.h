#ifndef _IOTSACONFIGMOD_H_
#define _IOTSACONFIGMOD_H_
#include "iotsa.h"
#include "iotsaApi.h"

class IotsaConfigMod : public IotsaModule {
public:
  IotsaConfigMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaModule(_app, _auth, true)
#ifdef IOTSA_WITH_HTTPS
  , newCertificate(NULL),
  newCertificateLength(0),
  newKey(NULL),
  newKeyLength(0)
#endif // IOTSA_WITH_HTTPS
  {
  }
	void setup() override;
	void lateSetup() override;
	void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  void uploadHandler();
  void uploadOkHandler();
private:
  void configLoad() override;
  void configSave() override;
#ifdef IOTSA_WITH_WEB
  void webHandler() override;
#endif
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* newCertificate;
  size_t newCertificateLength;
  const uint8_t* newKey;
  size_t newKeyLength;
#endif // IOTSA_WITH_HTTPS
};

#endif
