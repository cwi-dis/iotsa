#ifndef _IOTSACONFIGMOD_H_
#define _IOTSACONFIGMOD_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_API
#define IotsaConfigModBaseMod IotsaApiMod
#else
#define IotsaConfigModBaseMod IotsaMod
#endif

class IotsaConfigMod : public IotsaConfigModBaseMod {
public:
  IotsaConfigMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth=NULL)
  : IotsaConfigModBaseMod(_app, _auth, true) 
#ifdef IOTSA_WITH_HTTPS
  , newCertificate(NULL),
  newCertificateLength(0),
  newKey(NULL),
  newKeyLength(0)
#endif // IOTSA_WITH_HTTPS
  {
  }
	void setup() override;
	void serverSetup() override;
	void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
#ifdef IOTSA_WITH_API
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
#endif
  void uploadHandler();
  void uploadOkHandler();
private:
  void configLoad() override;
  void configSave() override;
  void handler();
  bool wifiDisabledChanged = false;
#ifdef IOTSA_WITH_BLE
  bool bleDisabledChanged = false;
#endif
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* newCertificate;
  size_t newCertificateLength;
  const uint8_t* newKey;
  size_t newKeyLength;
#endif // IOTSA_WITH_HTTPS
};

#endif
