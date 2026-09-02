#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"
#include "iotsaStatus.h"      // cwi-dis/iotsa#106: getBootReason/printHeapSpace/networkIsUp moved there
#include "iotsaController.h"  // cwi-dis/iotsa#106: the iotsa_mode state machine moved there
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

const char* IotsaConfig::getBootReason() {
  // Moved to IotsaStatus (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  return iotsaStatus.getBootReason();
}

// The iotsa_mode state machine now lives in IotsaController (cwi-dis/iotsa#106).
// These are deprecated forwarders, kept for one release for downstream code.
const char *IotsaConfig::modeName(iotsa_mode mode) { return iotsaController.modeName(mode); }
bool IotsaConfig::inConfigurationMode(bool extend) { return iotsaController.inConfigurationMode(extend); }
void IotsaConfig::extendCurrentMode() { iotsaController.extendCurrentMode(); }
void IotsaConfig::setExtensionCallback(extensionCallback ecmcb) { iotsaController.setExtensionCallback(ecmcb); }
void IotsaConfig::allowRequestedConfigurationMode() { iotsaController.allowRequestedConfigurationMode(); }
void IotsaConfig::allowRCMDescription(const char *desc) { iotsaController.allowRCMDescription(desc); }
// inConfigurationOrFactoryMode() removed (cwi-dis/iotsa#106): callers now use
// iotsaConfigSettingsWritable() (iotsaController.h).

uint32_t IotsaConfig::getStatusColor() {
  // Faithful translation of the old wifiMode switch onto the iotsaStatus bus
  // (cwi-dis/iotsa#106). The real LED-semantics rework (flash patterns, etc.) is
  // cwi-dis/iotsa#176.
  iotsa_mode mode = iotsaController.currentMode();
  if (mode == IOTSA_MODE_FACTORY_RESET) return 0x3f0000;   // Red: factory-reset mode
  if (!iotsaStatus.wifiEnabled) return 0;                   // radio disabled: LED off

  uint32_t extraColor = 0;
  if (!iotsaStatus.wifiStationConnected) {
    if (iotsaStatus.wifiApActive) {
      extraColor = 0x1f1f1f;      // white tint: serving our own AP (fallback / unconfigured)
    } else {
      return 0x3f1f00;           // Orange: hunting for WiFi
    }
  }
  if (mode == IOTSA_MODE_CONFIG) return extraColor | 0x3f003f;  // Magenta: configuration mode
  if (mode == IOTSA_MODE_OTA)    return extraColor | 0x003f3f;  // Cyan: OTA mode
  return extraColor; // Off when connected+normal; whiteish on the fallback AP
}

void IotsaConfig::pauseSleep() { 
  pauseSleepCount++; 
}

void IotsaConfig::resumeSleep() { 
  pauseSleepCount--; 
}

uint32_t IotsaConfig::postponeSleep(uint32_t ms) {
  uint32_t noSleepBefore = millis() + ms + activityExtraWakeDuration;
  if (noSleepBefore > postponeSleepMillis) postponeSleepMillis = noSleepBefore;
  int32_t rv = postponeSleepMillis - millis();
  if (rv < 2) rv = 0;
  return rv;
}

bool IotsaConfig::canSleep() {
  if (pauseSleepCount > 0) return false;
  if (millis() > postponeSleepMillis) postponeSleepMillis = 0;
  return postponeSleepMillis == 0;
}

void IotsaConfig::configLoad() {
  IotsaConfigFileLoad cf("/config/config.cfg");
  iotsaConfig.configWasLoaded = true;
  // The "mode" key moved to IotsaController's pendingmode.cfg mailbox (cwi-dis/iotsa#106).
  cf.get("hostName", iotsaConfig.hostName, "");
  if (iotsaConfig.hostName == "") iotsaConfig.setDefaultHostName();
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
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

void IotsaConfig::requestReboot(uint32_t ms) {
  // Moved to IotsaController (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  iotsaController.requestReboot(ms);
}

void IotsaConfig::printHeapSpace() {
  // Moved to IotsaStatus (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  iotsaStatus.printHeapSpace();
}

bool IotsaConfig::networkIsUp() {
  // Moved to IotsaStatus (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  return iotsaStatus.networkIsUp();
}