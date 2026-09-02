#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaFS.h"
#include "iotsaStatus.h"   // cwi-dis/iotsa#106: getBootReason/printHeapSpace/networkIsUp moved there
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

void IotsaConfig::loop() {
  if (rebootAtMillis && millis() > rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }

}

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

const char *IotsaConfig::modeName(iotsa_mode mode) {
#ifdef IOTSA_WITH_WEB
  if (mode == IOTSA_MODE_NORMAL)
    return "normal";
  if (mode == IOTSA_MODE_CONFIG)
    return "configuration";
  if (mode == IOTSA_MODE_OTA)
    return "OTA";
  if (mode == IOTSA_MODE_FACTORY_RESET)
    return "factory-reset";
#endif // IOTSA_WITH_WEB
  return "unknown";
}

bool IotsaConfig::inConfigurationMode(bool extend) { 
  bool ok = configurationMode == IOTSA_MODE_CONFIG;
  if (ok && extend) extendCurrentMode();
  return ok;
}

bool IotsaConfig::inConfigurationOrFactoryMode() { 
  if (configurationMode == IOTSA_MODE_CONFIG) return true;
  if (wifiMode == IOTSA_WIFI_FACTORY) return true;
  return false;
}

void IotsaConfig::extendCurrentMode() {
  IFDEBUG IotsaSerial.println("Configuration mode extended");
  configurationModeEndTime = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  // Allow interested module (probably IotsaBattery) to extend all sorts of timeers
  if (extendCurrentModeCallback) extendCurrentModeCallback();
#ifndef ESP32
  ESP.wdtFeed();
#endif

}

void IotsaConfig::setExtensionCallback(extensionCallback ecmcb) {
  extendCurrentModeCallback = ecmcb;
}

void IotsaConfig::endConfigurationMode() {
  IFDEBUG IotsaSerial.println("Configuration mode ended");
  configurationMode = IOTSA_MODE_NORMAL;
  configurationModeEndTime = 0;
  nextConfigurationMode = IOTSA_MODE_NORMAL;
  nextConfigurationModeEndTime = 0;
  configSave();
  wantWifiModeSwitchAtMillis = millis(); // need to tell wifi
}

void IotsaConfig::beginConfigurationMode() {
  IFDEBUG IotsaSerial.println("Configuration mode entered");
  configurationMode = IOTSA_MODE_CONFIG;
  configurationModeEndTime = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  // No need to tell wifi (or save config): this call is done only by the
  // WiFi module when switching from factory mode to having a WiFi.
}

void IotsaConfig::factoryReset() {
    IFDEBUG IotsaSerial.println("configurationMode: Factory-reset");
  	delay(1000);
  	IFDEBUG IotsaSerial.println("Formatting LittleFS...");
  	IOTSA_FS.format();
  	IFDEBUG IotsaSerial.println("Format done, rebooting.");
  	delay(2000);
  	ESP.restart();
}

void IotsaConfig::allowRequestedConfigurationMode() {
  if (nextConfigurationMode == configurationMode) return;
  IFDEBUG IotsaSerial.print("Switching configurationMode to ");
  IFDEBUG IotsaSerial.println(nextConfigurationMode);
  configurationMode = nextConfigurationMode;
  configurationModeEndTime = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
  nextConfigurationMode = IOTSA_MODE_NORMAL;
  nextConfigurationModeEndTime = 0;
  if (configurationMode == IOTSA_MODE_FACTORY_RESET) factoryReset();
  wantWifiModeSwitchAtMillis = millis(); // need to tell wifi
}

void IotsaConfig::allowRCMDescription(const char *_rcmInteractionDescription) {
  rcmInteractionDescription = _rcmInteractionDescription;
}

uint32_t IotsaConfig::getStatusColor() {
  if (configurationMode == IOTSA_MODE_FACTORY_RESET) return 0x3f0000; // Red: Factory reset mode
  uint32_t extraColor = 0;
  switch(wifiMode) {
  case IOTSA_WIFI_DISABLED:
    return 0;
  case IOTSA_WIFI_SEARCHING:
    return 0x3f1f00;  // Orange: searching for WiFi
  case IOTSA_WIFI_FACTORY:
  case IOTSA_WIFI_NOTFOUND:
    extraColor = 0x1f1f1f;  // Add a bit of white to the configuration mode color
    // Pass through
  default:
    // Pass through
    ;
  }
  if (configurationMode == IOTSA_MODE_CONFIG) return extraColor | 0x3f003f;	// Magenta: user-requested configuration mode
  if (configurationMode == IOTSA_MODE_OTA) return extraColor | 0x003f3f;	// Cyan: OTA mode
  return extraColor; // Off: all ok, whiteish: factory reset network
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
  int tcm;
  cf.get("mode", tcm, IOTSA_MODE_NORMAL);
  iotsaConfig.configurationMode = (iotsa_mode)tcm;
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
  cf.put("mode", nextConfigurationMode); // Note: nextConfigurationMode, which will be read as configurationMode
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
  IFDEBUG IotsaSerial.println("Restart requested");
  rebootAtMillis = millis() + ms;
}

void IotsaConfig::printHeapSpace() {
  // Moved to IotsaStatus (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  iotsaStatus.printHeapSpace();
}

bool IotsaConfig::networkIsUp() {
  // Moved to IotsaStatus (cwi-dis/iotsa#106). Deprecated forwarder for one release.
  return iotsaStatus.networkIsUp();
}