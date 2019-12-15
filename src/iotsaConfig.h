#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_

// Intended to be included from iotsa.h

typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } config_mode;

class IotsaConfig {
public:
  bool configWasLoaded = false;
  bool wifiEnabled = false;
  bool otaEnabled = false;
  bool disableWifiOnBoot = false;
  bool wifiPrivateNetworkMode = false;
  config_mode configurationMode = IOTSA_MODE_NORMAL;
  unsigned long configurationModeEndTime = 0;
  config_mode nextConfigurationMode = IOTSA_MODE_NORMAL;
  unsigned long nextConfigurationModeEndTime = 0;
  String hostName = "";
  int configurationModeTimeout = 0;
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* httpsCertificate;
  size_t httpsCertificateLength;
  const uint8_t* httpsKey;
  size_t httpsKeyLength;
#endif // IOTSA_WITH_HTTPS
  uint32_t postponeSleepMillis = 0;
  int pauseSleepCount = 0;

  void configLoad();
  void ensureConfigLoaded();
  const char* getBootReason();
  const char *modeName(config_mode mode);
  void setDefaultHostName();
  void setDefaultCertificate();
  bool usingDefaultCertificate();
  bool inConfigurationMode();
  bool inConfigurationOrFactoryMode();
  uint32_t getStatusColor();
  void pauseSleep();
  void resumeSleep();
  void postponeSleep(uint32_t ms);
  bool canSleep();
};

extern IotsaConfig iotsaConfig;
#endif
