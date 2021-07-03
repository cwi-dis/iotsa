#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_

#include <functional>

typedef std::function<void(void)> extensionCallback;

// Intended to be included from iotsa.h

typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } config_mode;
typedef enum { IOTSA_WIFI_DISABLED, IOTSA_WIFI_FACTORY, IOTSA_WIFI_NORMAL, IOTSA_WIFI_SEARCHING, IOTSA_WIFI_NOTFOUND} iotsa_wifi_mode;
class IotsaConfig {
  friend class IotsaConfigMod;
  friend class IotsaOtaMod;
  friend class IotsaWifiMod;
  friend class IotsaBatteryMod;
private:
  bool configWasLoaded = false;
  bool otaEnabled = false;
  iotsa_wifi_mode wifiMode = IOTSA_WIFI_DISABLED;
  bool wantWifiModeSwitch = false;
  config_mode configurationMode = IOTSA_MODE_NORMAL;
  unsigned long configurationModeEndTime = 0;
  config_mode nextConfigurationMode = IOTSA_MODE_NORMAL;
  unsigned long nextConfigurationModeEndTime = 0;
  int configurationModeTimeout = 0;
  uint32_t postponeSleepMillis = 0;
  int pauseSleepCount = 0;
  uint32_t rebootAtMillis = 0;
  extensionCallback extendCurrentModeCallback;
  void beginConfigurationMode();
  void endConfigurationMode();
  void factoryReset();
public:
  bool wifiEnabled = false;
  String hostName = "";
  bool disableWifiOnBoot = false;
  bool mdnsEnabled = false;
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* httpsCertificate;
  size_t httpsCertificateLength;
  const uint8_t* httpsKey;
  size_t httpsKeyLength;
#endif // IOTSA_WITH_HTTPS
  const char* rcmInteractionDescription = NULL;

public:
  void loop();
  void configLoad();
  void configSave();
  void ensureConfigLoaded();
  const char* getBootReason();
  const char *modeName(config_mode mode);
  void setDefaultHostName();
  void setDefaultCertificate();
  bool usingDefaultCertificate();
  bool inConfigurationMode(bool extend=false);
  bool inConfigurationOrFactoryMode();
  void extendConfigurationMode();
  void allowRequestedConfigurationMode();
  void allowRCMDescription(const char *_rcmInteractionDescription);
  uint32_t getStatusColor();
  void pauseSleep();
  void resumeSleep();
  uint32_t postponeSleep(uint32_t ms);
  bool canSleep();
  void requestReboot(uint32_t ms);
  void printHeapSpace();
  bool networkIsUp();
  void setExtensionCallback(extensionCallback ecmcb);
};

extern IotsaConfig iotsaConfig;
#endif
