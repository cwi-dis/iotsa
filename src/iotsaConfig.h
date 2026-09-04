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
  // Auto-expiry of a maintenance mode, in seconds. A persisted, user-edited knob
  // -- it belongs here, not on IotsaController (cwi-dis/iotsa#106 step 5b, undoing
  // 5262d1c's relocation). config.cfg "rebootTimeout" key; IotsaModeMachine reads
  // it; iotsaController.modeTimeout() / setModeTimeout() are forwarders.
  int configurationModeTimeout = 0;
#ifdef ESP32
  // Hardware-watchdog timeout in ms, 0 = off. Persisted knob (config.cfg), edited
  // via /config; IotsaController owns the timer (cwi-dis/iotsa#106 step 5d).
  uint32_t watchdogDuration = 0;
#endif
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
  void setDefaultHostName();
  void setDefaultCertificate();
  bool usingDefaultCertificate();

  // The iotsa_mode state machine, reboot request, sleep-inhibit bookkeeping and
  // radio state all moved off IotsaConfig in cwi-dis/iotsa#106 -- use
  // iotsaController.* (requestReboot / modeName / inConfigurationMode /
  // extendCurrentMode / allowRequestedConfigurationMode / allowRCMDescription)
  // and iotsaStatus.* (getBootReason / networkIsUp / printHeapSpace). The
  // one-release [[deprecated]] forwarders were removed in cwi-dis/iotsa#243.
  // getStatusColor() also moved in #243 -- it is iotsaStatus.statusColor() now
  // (a derived view of mode + wifi state, nothing to do with persisted config).
};

extern IotsaConfig iotsaConfig;
#endif
