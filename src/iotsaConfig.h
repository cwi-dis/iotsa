#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_

#include <functional>

typedef std::function<void(void)> extensionCallback;

// Intended to be included from iotsa.h

// The device mode: normally NORMAL (running the application). The other values
// are transient, deliberately-entered, auto-expiring windows in which normal
// operation is suspended so the device can be worked on. Distinct from the
// "runmode" concept (sleep/wake and radio rhythm) -- see cwi-dis/iotsa#106.
typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } iotsa_mode;
// iotsa_wifi_mode removed (cwi-dis/iotsa#106): WiFi state is IotsaWifiController's
// staState()/apState() plus the iotsaStatus.wifi* booleans.
typedef enum { IOTSA_BLE_DISABLED, IOTSA_BLE_ENABLED } iotsa_ble_mode;

class IotsaConfig {
  friend class IotsaConfigMod;
  friend class IotsaRunmodeMod;
  friend class IotsaController;
  friend class IotsaOtaMod;
  friend class IotsaWifiMod;
  friend class IotsaBLEServerMod;
  friend class IotsaBatteryMod;
private:
  bool configWasLoaded = false;
  bool otaEnabled = false;
  bool wifiDisabledOnBoot = false;
#ifdef IOTSA_WITH_BLE
  bool bleDisabledOnBoot = false;
  iotsa_ble_mode bleMode = IOTSA_BLE_DISABLED;
  uint32_t wantBleModeSwitchAtMillis = 0;
#endif
  // The iotsa_mode state machine moved to IotsaController (cwi-dis/iotsa#106).
  uint32_t postponeSleepMillis = 0;
  uint32_t activityExtraWakeDuration = 0;
  int pauseSleepCount = 0;
public:
  // Mode-machine setting: still persisted in config.cfg (rebootTimeout key) and
  // edited via /config; IotsaController reads it. cwi-dis/iotsa#106 will move it.
  int configurationModeTimeout = 0;
  // wifiEnabled / wifiStationConnected / wifiApActive / mdnsEnabled moved to
  // IotsaStatus (cwi-dis/iotsa#106). Use iotsaStatus.* instead.
  String hostName = "";
#ifdef IOTSA_WITH_HTTPS
  const uint8_t* httpsCertificate;
  size_t httpsCertificateLength;
  const uint8_t* httpsKey;
  size_t httpsKeyLength;
#endif // IOTSA_WITH_HTTPS

public:
  void configLoad();
  void configSave();
  void ensureConfigLoaded();
  [[deprecated("moved to iotsaStatus.getBootReason()")]]
  const char* getBootReason();
  void setDefaultHostName();
  void setDefaultCertificate();
  bool usingDefaultCertificate();
  uint32_t getStatusColor();
  void pauseSleep();
  void resumeSleep();
  uint32_t postponeSleep(uint32_t ms);
  bool canSleep();

  // ---- moved to IotsaController (cwi-dis/iotsa#106); deprecated forwarders ----
  [[deprecated("moved to iotsaController.requestReboot()")]]
  void requestReboot(uint32_t ms);
  [[deprecated("moved to iotsaController.modeName()")]]
  const char *modeName(iotsa_mode mode);
  [[deprecated("moved to iotsaController.inConfigurationMode()")]]
  bool inConfigurationMode(bool extend=false);
  [[deprecated("moved to iotsaController.extendCurrentMode()")]]
  void extendCurrentMode();
  [[deprecated("moved to iotsaController.allowRequestedConfigurationMode()")]]
  void allowRequestedConfigurationMode();
  [[deprecated("moved to iotsaController.allowRCMDescription()")]]
  void allowRCMDescription(const char *_rcmInteractionDescription);
  [[deprecated("moved to iotsaController.setExtensionCallback()")]]
  void setExtensionCallback(extensionCallback ecmcb);

  // ---- moved to IotsaStatus (cwi-dis/iotsa#106); deprecated forwarders ----
  [[deprecated("moved to iotsaStatus.printHeapSpace()")]]
  void printHeapSpace();
  [[deprecated("moved to iotsaStatus.networkIsUp()")]]
  bool networkIsUp();
};

extern IotsaConfig iotsaConfig;
#endif
