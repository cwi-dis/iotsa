#include "iotsa.h"
#include "iotsaConfigFile.h"

#ifdef ESP32
#include <esp_log.h>
#include <rom/rtc.h>
#endif

//
// Global variable initialization
//
IotsaConfig iotsaConfig;


#ifdef IOTSA_WITH_HTTPS
#include "iotsaConfigDefaultCert512.h"
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
  static const char *reason = NULL;
  if (reason == NULL) {
    reason = "unknown";
#ifndef ESP32
    rst_info *rip = ESP.getResetInfoPtr();
    static const char *reasons[] = {
      "power",
      "hardwareWatchdog",
      "exception",
      "softwareWatchdog",
      "softwareReboot",
      "deepSleepAwake",
      "externalReset"
    };
    if (rip->reason < sizeof(reasons)/sizeof(reasons[0])) {
      reason = reasons[(int)rip->reason];
    }
#else
  RESET_REASON r = rtc_get_reset_reason(0);
  RESET_REASON r2 = rtc_get_reset_reason(1);
  // Determine best reset reason
  if (r == TG0WDT_SYS_RESET || r == RTCWDT_RTC_RESET) r = r2;
  static const char *reasons[] = {
    "0",
    "power",
    "2",
    "softwareReboot",
    "legacyWatchdog",
    "deepSleepAwake",
    "sdio",
    "tg0Watchdog",
    "tg1Watchdog",
    "rtcWatchdog",
    "intrusion",
    "tgWatchdogCpu",
    "softwareRebootCpu",
    "rtcWatchdogCpu",
    "externalReset",
    "brownout",
    "rtcWatchdogRtc"
  };
  if ((int)r < sizeof(reasons)/sizeof(reasons[0])) {
    reason = reasons[(int)r];
  }
#endif
  }
  return reason;
}

const char *IotsaConfig::modeName(config_mode mode) {
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

bool IotsaConfig::inConfigurationMode() { 
  return configurationMode == IOTSA_MODE_CONFIG; 
}

bool IotsaConfig::inConfigurationOrFactoryMode() { 
  if (configurationMode == IOTSA_MODE_CONFIG) return true;
  if (wifiMode == IOTSA_WIFI_FACTORY) return true;
  return false;
}

void IotsaConfig::extendConfigurationMode() {
  iotsaConfig.configurationModeEndTime = millis() + 1000*CONFIGURATION_MODE_TIMEOUT;
}

void IotsaConfig::allowRequestedConfigurationMode() {
  IotsaSerial.println("xxxjack allowRequestedConfigurationMode not implemented");
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

void IotsaConfig::postponeSleep(uint32_t ms) {
  uint32_t noSleepBefore = millis() + ms;
  if (noSleepBefore > postponeSleepMillis) postponeSleepMillis = noSleepBefore;
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
  iotsaConfig.configurationMode = (config_mode)tcm;
  cf.get("hostName", iotsaConfig.hostName, "");
  if (iotsaConfig.hostName == "") iotsaConfig.setDefaultHostName();
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
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

void IotsaConfig::ensureConfigLoaded() { 
  if (!configWasLoaded) configLoad(); 
};

void IotsaConfig::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  rebootAtMillis = millis() + ms;
}