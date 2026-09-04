#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"
// No iotsaStatus.h / iotsaController.h here any more: since getStatusColor() moved
// out (cwi-dis/iotsa#243) IotsaConfig touches neither the status bus nor the
// controller. (iotsa.h still pulls both in for other translation units.)
#ifdef ESP32
#include <esp_log.h>
#include <rom/rtc.h>
#endif

//
// Global variable initialization
//
IotsaConfig iotsaConfig;


#ifdef IOTSA_WITH_HTTPS
#include "iotsaConfigDefaultCert.h"
#endif

// IotsaConfig::loop() and the deferred-reboot timer moved to IotsaController
// (cwi-dis/iotsa#106). requestReboot() below is a deprecated forwarder.

void IotsaConfig::setDefaultHostName() {
  hostName = "iotsa";
#ifdef ESP32
  hostName += String(uint32_t(ESP.getEfuseMac()), HEX);
#else
  hostName += String(ESP.getChipId(), HEX);
#endif
}

void IotsaConfig::setDefaultCertificate() {
#ifdef IOTSA_WITH_HTTPS
  httpsCertificate = defaultHttpsCertificate;
  httpsCertificateLength = sizeof(defaultHttpsCertificate);
  httpsKey = defaultHttpsKey;
  httpsKeyLength = sizeof(defaultHttpsKey);
  IFDEBUG IotsaSerial.print("Default https key len=");
  IFDEBUG IotsaSerial.print(httpsKeyLength);
  IFDEBUG IotsaSerial.print(", cert len=");
  IFDEBUG IotsaSerial.println(httpsCertificateLength);
#endif // IOTSA_WITH_HTTPS
}

bool IotsaConfig::usingDefaultCertificate() {
#ifdef IOTSA_WITH_HTTPS
  return httpsKey == defaultHttpsKey;
#else
  return false;
#endif
}

// getBootReason / modeName / inConfigurationMode / extendCurrentMode /
// allowRequestedConfigurationMode / allowRCMDescription moved to IotsaController /
// IotsaStatus in cwi-dis/iotsa#106; the one-release [[deprecated]] forwarders were
// removed in cwi-dis/iotsa#243. inConfigurationOrFactoryMode() is gone too --
// callers use iotsaConfigSettingsWritable() (iotsaController.h). getStatusColor()
// moved to IotsaStatus::statusColor() in #243 -- it is a derived view of mode +
// wifi state, no IotsaConfig state involved; this class no longer depends on
// IotsaController at all.

// pauseSleep() / resumeSleep() / postponeSleep() / canSleep() moved to
// IotsaSleepPolicy (cwi-dis/iotsa#106); no forwarder, callers renamed to
// iotsaController.*.

void IotsaConfig::configLoad() {
  IotsaConfigFileLoad cf("/config/config.cfg");
  iotsaConfig.configWasLoaded = true;
  // The "mode" key moved to IotsaController's pendingmode.cfg mailbox (cwi-dis/iotsa#106).
  cf.get("hostName", iotsaConfig.hostName, "");
  if (iotsaConfig.hostName == "") iotsaConfig.setDefaultHostName();
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
#ifdef ESP32
  cf.get("watchdogDuration", iotsaConfig.watchdogDuration, 0);
#endif
  cf.get("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, false);
#ifdef IOTSA_WITH_BLE
  cf.get("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, false);
#endif
#ifdef IOTSA_WITH_HTTPS
  if (iotsaConfigFileExists("/config/httpsKey.der") && iotsaConfigFileExists("/config/httpsCert.der")) {
    bool ok = iotsaConfigFileLoadBinary("/config/httpsKey.der", (uint8_t **)&iotsaConfig.httpsKey, &iotsaConfig.httpsKeyLength);
    if (ok) {
      IFDEBUG IotsaSerial.println("Loaded /config/httpsKey.der");
    }
    ok = iotsaConfigFileLoadBinary("/config/httpsCert.der", (uint8_t **)&iotsaConfig.httpsCertificate, &iotsaConfig.httpsCertificateLength);
    if (ok) {
      IFDEBUG IotsaSerial.println("Loaded /config/httpsCert.der");
    }
  }
#endif // IOTSA_WITH_HTTPS
}

void IotsaConfig::configSave() {
  IotsaConfigFileSave cf("/config/config.cfg");
  // The "mode" key moved to IotsaController's pendingmode.cfg mailbox (cwi-dis/iotsa#106).
  cf.put("hostName", hostName);
  cf.put("rebootTimeout", configurationModeTimeout);
#ifdef ESP32
  cf.put("watchdogDuration", watchdogDuration);
#endif
  cf.put("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot);
#ifdef IOTSA_WITH_BLE
  cf.put("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot);
#endif
  // Key/cert are saved in iotsaConfigMod
  IFDEBUG IotsaSerial.println("Saved config.cfg");
}
void IotsaConfig::ensureConfigLoaded() { 
  if (!configWasLoaded) configLoad(); 
};

// requestReboot / printHeapSpace / networkIsUp forwarders removed in cwi-dis/iotsa#243
// (were moved to IotsaController / IotsaStatus in #106).