#ifndef _IOTSACONFIG_H_
#define _IOTSACONFIG_H_

#include <stdint.h>   // uint32_t / uint8_t (was pulled in transitively via <functional>)

// Intended to be included from iotsa.h

// The device mode: normally NORMAL (running the application). The other values
// are transient, deliberately-entered, auto-expiring windows in which normal
// operation is suspended so the device can be worked on. Distinct from the
// "runmode" concept (sleep/wake and radio rhythm) -- see cwi-dis/iotsa#106.
typedef enum { IOTSA_MODE_NORMAL, IOTSA_MODE_CONFIG, IOTSA_MODE_OTA, IOTSA_MODE_FACTORY_RESET } iotsa_mode;
// iotsa_wifi_mode / iotsa_ble_mode removed (cwi-dis/iotsa#106): radio state is
// IotsaController's {wifi,ble}RadioWanted() policy plus the iotsaStatus.wifi*
// booleans; IotsaBLEServerMod reconciles advertising with it.

class IotsaConfig {
  friend class IotsaConfigMod;
  friend class IotsaRunmodeMod;
  friend class IotsaController;
  friend class IotsaWifiMod;
  friend class IotsaBLEServerMod;
  friend class IotsaBatteryMod;
private:
  bool configWasLoaded = false;
  bool wifiDisabledOnBoot = false;
#ifdef IOTSA_WITH_BLE
  bool bleDisabledOnBoot = false;   // boot policy; IotsaController seeds _bleRadioEnabled from it
#endif
  // The iotsa_mode state machine, and the sleep-inhibit bookkeeping
  // (postponeSleepMillis / activityExtraWakeDuration / pauseSleepCount plus
  // pauseSleep()/postponeSleep()/canSleep()), moved to IotsaController
  // (cwi-dis/iotsa#106). Use iotsaController.postponeSleep() etc.
public:
  // configurationModeTimeout moved to iotsaController.modeTimeout() (cwi-dis/iotsa#106);
  // config.cfg's "rebootTimeout" key still persists it, via IotsaConfig::config{Load,Save}.
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
  // setExtensionCallback() deleted, not forwarded (cwi-dis/iotsa#106): the
  // extendCurrentMode callback mechanism is gone, and it had no downstream users.

  // ---- moved to IotsaStatus (cwi-dis/iotsa#106); deprecated forwarders ----
  [[deprecated("moved to iotsaStatus.printHeapSpace()")]]
  void printHeapSpace();
  [[deprecated("moved to iotsaStatus.networkIsUp()")]]
  bool networkIsUp();
};

extern IotsaConfig iotsaConfig;
#endif
