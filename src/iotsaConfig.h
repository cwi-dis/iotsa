#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_
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
    singleton = this;
  }
	void setup();
	void serverSetup();
	void loop();
  String info();
  static IotsaConfigMod *singleton;
protected:
  bool getHandler(const char *path, JsonObject& reply);
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
  void uploadHandler();
  void uploadOkHandler();
private:
  void configLoad();
  void configSave();
  void handler();
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* newCertificate;
  size_t newCertificateLength;
  const uint8_t* newKey;
  size_t newKeyLength;
#endif // IOTSA_WITH_HTTPS
};

#endif
