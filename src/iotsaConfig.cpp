#include <Esp.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <esp_log.h>
#include <rom/rtc.h>
#else
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif
#include <FS.h>

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaConfig.h"

IotsaConfigMod *IotsaConfigMod::singleton = 0;

#ifdef IOTSA_WITH_HTTPS
#include <libb64/cdecode.h>
#include <libb64/cencode.h>
#endif // IOTSA_WITH_HTTPS

//
// Global variables, because other modules need them too.
//
IotsaConfig iotsaConfig = {
  false,
  false,
  false,
  IOTSA_MODE_NORMAL,
  0,
  IOTSA_MODE_NORMAL,
  0,
  ""
};

#ifdef IOTSA_WITH_HTTPS
// Default key and certificate for https service
// The certificate is stored in PMEM
static const uint8_t defaultHttpsCertificate[] PROGMEM = {
  0x30, 0x82, 0x01, 0xc3, 0x30, 0x82, 0x01, 0x2c, 0x02, 0x09, 0x00, 0x9a,
  0x94, 0x9a, 0x76, 0xf8, 0x23, 0x56, 0xdd, 0x30, 0x0d, 0x06, 0x09, 0x2a,
  0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x26,
  0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x05, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0b, 0x31, 0x39, 0x32, 0x2e, 0x31, 0x36, 0x38, 0x2e, 0x34,
  0x2e, 0x31, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x38, 0x30, 0x35, 0x32, 0x38,
  0x30, 0x39, 0x32, 0x35, 0x35, 0x37, 0x5a, 0x17, 0x0d, 0x33, 0x32, 0x30,
  0x32, 0x30, 0x34, 0x30, 0x39, 0x32, 0x35, 0x35, 0x37, 0x5a, 0x30, 0x26,
  0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x05, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0b, 0x31, 0x39, 0x32, 0x2e, 0x31, 0x36, 0x38, 0x2e, 0x34,
  0x2e, 0x31, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
  0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00,
  0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xd3, 0x10, 0x98, 0x43, 0xf6,
  0x95, 0xed, 0xda, 0x06, 0x6f, 0xa4, 0xb6, 0xc3, 0x05, 0x04, 0x3e, 0xb6,
  0x4c, 0x51, 0xe1, 0x55, 0x81, 0x5a, 0xe4, 0xa1, 0xc4, 0xb0, 0x35, 0xea,
  0x4b, 0xc4, 0x17, 0x47, 0xf2, 0xdb, 0x92, 0xde, 0x7e, 0x65, 0x06, 0x76,
  0xff, 0x79, 0xe3, 0x8b, 0xa9, 0x87, 0xca, 0x29, 0x07, 0x6c, 0xcd, 0x1b,
  0xe9, 0xca, 0xb8, 0x11, 0x08, 0x03, 0xcb, 0xc0, 0x08, 0xbd, 0xdf, 0xa5,
  0x33, 0xee, 0x15, 0x17, 0x99, 0x5f, 0x38, 0x63, 0xc5, 0x49, 0x4e, 0xb3,
  0x0f, 0x0b, 0x45, 0x33, 0xe9, 0xd4, 0x72, 0xf1, 0xed, 0xcc, 0xd7, 0xd8,
  0xdc, 0x61, 0xc9, 0x03, 0x8b, 0xef, 0x9a, 0xfe, 0x20, 0x8a, 0xc1, 0x53,
  0xc3, 0x3b, 0x12, 0xfe, 0xd4, 0xd5, 0xa0, 0x2c, 0x15, 0x17, 0xec, 0x5d,
  0xe2, 0x4b, 0x6f, 0xae, 0xf6, 0x8c, 0xf7, 0x6b, 0x03, 0x16, 0x7a, 0xfe,
  0x39, 0xf9, 0x1d, 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03,
  0x81, 0x81, 0x00, 0x17, 0x65, 0x8c, 0xde, 0x92, 0xe1, 0xe2, 0x21, 0xba,
  0x34, 0xc6, 0xc0, 0x5e, 0x43, 0xf2, 0xc7, 0x01, 0x0f, 0xaf, 0x57, 0x62,
  0xae, 0xfa, 0x9c, 0xa6, 0xb7, 0xa7, 0x23, 0x70, 0xcd, 0x87, 0x8f, 0x95,
  0x54, 0x27, 0xbd, 0xc6, 0xa7, 0x46, 0x44, 0x87, 0xf1, 0x27, 0xff, 0x05,
  0xfd, 0x9e, 0xf3, 0xba, 0xe4, 0x3f, 0xbf, 0xdb, 0x6f, 0x2e, 0x14, 0xd5,
  0x85, 0x83, 0x01, 0x39, 0x42, 0x4b, 0x18, 0xcb, 0x0e, 0x40, 0x38, 0x14,
  0x30, 0x58, 0x70, 0xd0, 0xd4, 0xa3, 0xad, 0xbb, 0xcb, 0xbb, 0xb3, 0xa5,
  0x61, 0xea, 0x8e, 0xf1, 0x39, 0xd0, 0xdd, 0xf2, 0x11, 0xad, 0x90, 0xb7,
  0xe7, 0x76, 0xf7, 0xeb, 0x00, 0xe9, 0x07, 0x86, 0x6b, 0x07, 0xd9, 0x8b,
  0x4b, 0xbf, 0x69, 0x41, 0x5e, 0x97, 0x90, 0x48, 0xe3, 0x09, 0x81, 0x99,
  0x4d, 0x57, 0x28, 0xb2, 0x09, 0x0d, 0x57, 0x19, 0xf3, 0xc6, 0xed
};

// And so is the key.  These could also be in DRAM
static const uint8_t defaultHttpsKey[] PROGMEM = {
  0x30, 0x82, 0x02, 0x5d, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xd3,
  0x10, 0x98, 0x43, 0xf6, 0x95, 0xed, 0xda, 0x06, 0x6f, 0xa4, 0xb6, 0xc3,
  0x05, 0x04, 0x3e, 0xb6, 0x4c, 0x51, 0xe1, 0x55, 0x81, 0x5a, 0xe4, 0xa1,
  0xc4, 0xb0, 0x35, 0xea, 0x4b, 0xc4, 0x17, 0x47, 0xf2, 0xdb, 0x92, 0xde,
  0x7e, 0x65, 0x06, 0x76, 0xff, 0x79, 0xe3, 0x8b, 0xa9, 0x87, 0xca, 0x29,
  0x07, 0x6c, 0xcd, 0x1b, 0xe9, 0xca, 0xb8, 0x11, 0x08, 0x03, 0xcb, 0xc0,
  0x08, 0xbd, 0xdf, 0xa5, 0x33, 0xee, 0x15, 0x17, 0x99, 0x5f, 0x38, 0x63,
  0xc5, 0x49, 0x4e, 0xb3, 0x0f, 0x0b, 0x45, 0x33, 0xe9, 0xd4, 0x72, 0xf1,
  0xed, 0xcc, 0xd7, 0xd8, 0xdc, 0x61, 0xc9, 0x03, 0x8b, 0xef, 0x9a, 0xfe,
  0x20, 0x8a, 0xc1, 0x53, 0xc3, 0x3b, 0x12, 0xfe, 0xd4, 0xd5, 0xa0, 0x2c,
  0x15, 0x17, 0xec, 0x5d, 0xe2, 0x4b, 0x6f, 0xae, 0xf6, 0x8c, 0xf7, 0x6b,
  0x03, 0x16, 0x7a, 0xfe, 0x39, 0xf9, 0x1d, 0x02, 0x03, 0x01, 0x00, 0x01,
  0x02, 0x81, 0x80, 0x23, 0x48, 0xc6, 0xeb, 0xb5, 0xb1, 0x62, 0xcd, 0xeb,
  0xfd, 0x85, 0xff, 0xb7, 0xa2, 0x83, 0x0f, 0x28, 0xd2, 0xa1, 0x6d, 0x96,
  0x29, 0xc5, 0xd4, 0x2a, 0xe7, 0x02, 0xbe, 0x40, 0xa8, 0xe1, 0xe3, 0x32,
  0x77, 0xfb, 0x15, 0x16, 0x74, 0xf2, 0xd6, 0x9d, 0xd6, 0x1f, 0xbe, 0x56,
  0x7e, 0xc4, 0xe0, 0x9a, 0xf3, 0x4e, 0xd1, 0x0b, 0x35, 0x8e, 0x5a, 0x2f,
  0x1e, 0xb3, 0xe3, 0xbf, 0xfa, 0xb6, 0x22, 0xfb, 0x76, 0xfa, 0x64, 0x9f,
  0x1b, 0xa7, 0xd5, 0xde, 0x97, 0xf5, 0xf2, 0xa4, 0x62, 0x50, 0x33, 0x8f,
  0x2e, 0xf6, 0x79, 0xca, 0x4f, 0xf3, 0x9a, 0x35, 0xc8, 0x43, 0x5a, 0x43,
  0x64, 0x05, 0xf7, 0x16, 0xac, 0x3a, 0x88, 0xa5, 0xe7, 0xba, 0x25, 0xb8,
  0x7e, 0x0d, 0x45, 0x08, 0xf0, 0x56, 0xd6, 0xed, 0xdc, 0x7b, 0x20, 0xc8,
  0x8a, 0x8e, 0x6a, 0x11, 0x12, 0x76, 0x9e, 0xaa, 0xa4, 0x60, 0xd1, 0x02,
  0x41, 0x00, 0xe8, 0xb7, 0xc8, 0x34, 0x37, 0x3f, 0x70, 0x9e, 0x72, 0x65,
  0xdc, 0x27, 0x72, 0xf9, 0xdd, 0xfb, 0xe5, 0x71, 0xcb, 0x6c, 0x86, 0xa4,
  0x2e, 0x15, 0x3a, 0xc8, 0x12, 0x09, 0x97, 0x02, 0xeb, 0xe2, 0x99, 0x3a,
  0xed, 0xc3, 0xca, 0xe7, 0x1d, 0x8c, 0x17, 0x27, 0x7b, 0x99, 0x5b, 0xe9,
  0x2a, 0x6a, 0xda, 0x85, 0x31, 0x07, 0xe2, 0x4b, 0x35, 0x7c, 0xb2, 0xfd,
  0x54, 0x9b, 0x09, 0x75, 0xa8, 0x13, 0x02, 0x41, 0x00, 0xe8, 0x2e, 0x3f,
  0x97, 0x30, 0xe9, 0x54, 0xe8, 0xdd, 0x24, 0x40, 0x22, 0xcd, 0x90, 0xe9,
  0x1b, 0x56, 0x12, 0xf6, 0x08, 0xbc, 0xbf, 0x87, 0x28, 0x16, 0x7f, 0x65,
  0x47, 0x71, 0xcf, 0x2c, 0xc5, 0xa6, 0xb6, 0x4a, 0xb8, 0x8c, 0x1f, 0x50,
  0xb6, 0x3a, 0xdd, 0xb9, 0xa9, 0x93, 0xc2, 0xe0, 0xe7, 0xbf, 0x49, 0xd3,
  0x87, 0x95, 0xd2, 0x27, 0x6c, 0x86, 0x31, 0x81, 0x8b, 0x6f, 0x3a, 0x60,
  0x0f, 0x02, 0x41, 0x00, 0xe4, 0xff, 0x1b, 0x91, 0x4e, 0x20, 0x2e, 0x08,
  0xac, 0x57, 0x51, 0x38, 0xbc, 0x69, 0xe5, 0xa7, 0x1e, 0x93, 0x48, 0x72,
  0x45, 0x57, 0x3f, 0x45, 0x82, 0xaf, 0x27, 0x18, 0xaa, 0xb1, 0xa4, 0x3e,
  0x39, 0x3c, 0x04, 0x85, 0x5a, 0x9e, 0xeb, 0xb3, 0x53, 0x81, 0x75, 0x9d,
  0x66, 0x14, 0xdd, 0xb9, 0x81, 0xc7, 0xf8, 0x84, 0x62, 0x88, 0x51, 0x5c,
  0xa0, 0xa0, 0xa9, 0xe5, 0x59, 0x7c, 0x9e, 0x63, 0x02, 0x40, 0x5b, 0x50,
  0x6d, 0x24, 0x1a, 0x51, 0x7a, 0x5a, 0x87, 0x36, 0xcd, 0x9e, 0xa2, 0x78,
  0x7e, 0xa5, 0x88, 0xa9, 0xb6, 0x67, 0xe3, 0x4b, 0xf4, 0x6d, 0x18, 0xc4,
  0x0b, 0xe2, 0x18, 0x69, 0xa1, 0xb3, 0x2e, 0x88, 0xfd, 0x44, 0x2d, 0x9f,
  0xd7, 0x4f, 0x84, 0x41, 0x55, 0xd6, 0xd2, 0xcd, 0x4f, 0x44, 0xf5, 0xdf,
  0xa4, 0x38, 0xeb, 0xfc, 0x96, 0x12, 0xc1, 0x88, 0x50, 0xe9, 0xb4, 0xda,
  0x21, 0x93, 0x02, 0x41, 0x00, 0x8e, 0x42, 0x18, 0x8d, 0x56, 0x0d, 0x9a,
  0xd9, 0xc1, 0xc6, 0xa0, 0x1b, 0x48, 0x86, 0xbb, 0xbf, 0xc6, 0xb2, 0xda,
  0x82, 0x8e, 0xc8, 0x22, 0xa4, 0x8c, 0x78, 0xa7, 0x40, 0x13, 0x3b, 0xc3,
  0x32, 0x7c, 0xfd, 0xa3, 0x70, 0xa3, 0xec, 0xd8, 0xf6, 0x97, 0x19, 0xbb,
  0x22, 0x0b, 0x3f, 0xfc, 0xd5, 0xe3, 0xd6, 0x6c, 0x5c, 0x85, 0xda, 0x5b,
  0x69, 0x28, 0xf8, 0x07, 0x20, 0x80, 0x69, 0xdf, 0x37
};

#endif

#if 0
String& hostName(iotsaConfig.hostName);
#endif

static unsigned long rebootAt;

static void wifiDefaultHostName() {
  iotsaConfig.hostName = "iotsa";
#ifdef ESP32
  iotsaConfig.hostName += String(uint32_t(ESP.getEfuseMac()), HEX);
#else
  iotsaConfig.hostName += String(ESP.getChipId(), HEX);
#endif
}

static const char* getBootReason() {
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

#ifdef IOTSA_WITH_WEB
static const char *modeName(config_mode mode) {
  if (mode == IOTSA_MODE_NORMAL)
    return "normal";
  if (mode == IOTSA_MODE_CONFIG)
    return "configuration";
  if (mode == IOTSA_MODE_OTA)
    return "OTA";
  if (mode == IOTSA_MODE_FACTORY_RESET)
    return "factory-reset";
  return "unknown";
}
#endif // IOTSA_WITH_WEB

void IotsaConfigMod::setup() {
  IFDEBUG IotsaSerial.print("boot reason: ");
  IFDEBUG IotsaSerial.println(getBootReason());
#ifdef IOTSA_WITH_HTTPS
  iotsaConfig.httpsCertificate = defaultHttpsCertificate;
  iotsaConfig.httpsCertificateLength = sizeof(defaultHttpsCertificate);
  iotsaConfig.httpsKey = defaultHttpsKey;
  iotsaConfig.httpsKeyLength = sizeof(defaultHttpsKey);
  IFDEBUG IotsaSerial.print("Default https key len=");
  IFDEBUG IotsaSerial.print(iotsaConfig.httpsKeyLength);
  IFDEBUG IotsaSerial.print(", cert len=");
  IFDEBUG IotsaSerial.println(iotsaConfig.httpsCertificateLength);
#endif // IOTSA_WITH_HTTPS
  configLoad();
  if (app.status) app.status->showStatus();
  if (iotsaConfig.configurationMode) {
  	IFDEBUG IotsaSerial.println("tmpConfigMode, re-saving config.cfg without it");
  	configSave();
    iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
    IFDEBUG IotsaSerial.print("tempConfigMode=");
    IFDEBUG IotsaSerial.print((int)iotsaConfig.configurationMode);
    IFDEBUG IotsaSerial.print(", timeout at ");
    IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
}
  // If a configuration mode was requested but the reset reason was not
  // external reset (the button) or powerup we do not honor the configuration mode
  // request: it could be triggered through a software bug or so, and we want to require
  // user interaction.
#ifndef ESP32
  rst_info *rip = ESP.getResetInfoPtr();
  int reason = (int)rip->reason;
  bool badReason = rip->reason != REASON_DEFAULT_RST && rip->reason != REASON_EXT_SYS_RST;
#else
  int reason = rtc_get_reset_reason(0);
  // xxxjack Not sure why I sometimes see the WDT reset on pressing the reset button...
  bool badReason = reason != POWERON_RESET && reason != RTCWDT_RTC_RESET;
#endif
  if (badReason && iotsaConfig.configurationMode != IOTSA_MODE_NORMAL) {
    iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
    IFDEBUG IotsaSerial.print("tmpConfigMode not honoured because of reset reason:");
    IFDEBUG IotsaSerial.println(reason);
  }
  // If factory reset is requested format the Flash and reboot
  if (iotsaConfig.configurationMode == IOTSA_MODE_FACTORY_RESET) {
  	IFDEBUG IotsaSerial.println("Factory-reset requested");
  	delay(1000);
  	IFDEBUG IotsaSerial.println("Formatting SPIFFS...");
  	SPIFFS.format();
  	IFDEBUG IotsaSerial.println("Format done, rebooting.");
  	delay(2000);
  	ESP.restart();
  }
  if (app.status) app.status->showStatus();
}

#ifdef IOTSA_WITH_WEB
void
IotsaConfigMod::handler() {
  bool wrongMode = false;
  bool anyChanged = false;
  bool hostnameChanged = false;
  if( server->hasArg("hostName")) {
    String argValue = server->arg("hostName");
    if (argValue != iotsaConfig.hostName) {
      if (iotsaConfig.inConfigurationOrFactoryMode()) {
        if (needsAuthentication("config")) return;
        iotsaConfig.hostName = argValue;
        anyChanged = true;
        hostnameChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  if( server->hasArg("rebootTimeout")) {
    int newValue = server->arg("rebootTimeout").toInt();
    if (newValue != iotsaConfig.configurationModeTimeout) {
      if (iotsaConfig.inConfigurationMode()) {
        if (needsAuthentication("config")) return;
        iotsaConfig.configurationModeTimeout = newValue;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  if( server->hasArg("mode")) {
    String argValue = server->arg("mode");
    if (argValue != "0") {
      if (needsAuthentication("config")) return;
      iotsaConfig.nextConfigurationMode = config_mode(atoi(argValue.c_str()));
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      anyChanged = true;
    }
  }
#ifdef IOTSA_WITH_HTTPS
  if (server->hasArg("httpsKey") && server->arg("httpsKey") != "") {
    if (iotsaConfig.inConfigurationMode()) {
      if (needsAuthentication("config")) return;
      String b64String(server->arg("httpsKey"));
      if (b64String.startsWith("-----BEGIN RSA PRIVATE KEY-----")) {
        // Strip DER header and footer
        int first = b64String.indexOf('\n');
        int last = b64String.lastIndexOf("-----END RSA PRIVATE KEY-----");
        if (first >= 0 && last >= 0) {
          b64String = b64String.substring(first, last);
        } else {
          IFDEBUG IotsaSerial.println("httpsKey bad format PEM");
          b64String = "";
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsKey bad PEM header");
        b64String = "";
      }
      const char *b64Value = b64String.c_str();
      int b64len = strlen(b64Value);
      int expDecodeLen = base64_decode_expected_len(b64len);
      char *tmpValue = (char *)malloc(expDecodeLen);
      if (tmpValue) {
        int decLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decLen > 0) {
          newKey = (uint8_t *)tmpValue;
          newKeyLength = decLen;
          IFDEBUG IotsaSerial.print("Decoded httpsKey len=");
          IFDEBUG IotsaSerial.print(decLen);
          IFDEBUG IotsaSerial.print(" expLen=");
          IFDEBUG IotsaSerial.print(expDecodeLen);
          IFDEBUG IotsaSerial.print(" b64len=");
          IFDEBUG IotsaSerial.println(b64len);
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("Error base64 decoding httpsKey");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsKey malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
  if (server->hasArg("httpsCertificate") && server->arg("httpsCertificate") != "") {
    if (iotsaConfig.inConfigurationMode()) {
      if (needsAuthentication("config")) return;
      String b64String(server->arg("httpsCertificate"));
      if (b64String.startsWith("-----BEGIN CERTIFICATE-----")) {
        // Strip DER header and footer
        int first = b64String.indexOf('\n');
        int last = b64String.lastIndexOf("-----END CERTIFICATE-----");
        if (first >= 0 && last >= 0) {
          b64String = b64String.substring(first, last);
        } else {
          IFDEBUG IotsaSerial.println("httpsCertificate bad format PEM");
          b64String = "";
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsCertificate bad PEM header");
        b64String = "";
      }
      const char *b64Value = b64String.c_str();
      int b64len = strlen(b64Value);
      int expDecodeLen = base64_decode_expected_len(b64len);
      IFDEBUG IotsaSerial.print("httpsCertificate expected len=");
      IFDEBUG IotsaSerial.println(expDecodeLen);
      char *tmpValue = (char *)malloc(expDecodeLen);
      if (tmpValue) {
        int decLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decLen > 0) {
          newCertificate = (uint8_t *)tmpValue;
          newCertificateLength = decLen;
          IFDEBUG IotsaSerial.print("Decoded httpsCertificate len=");
          IFDEBUG IotsaSerial.print(decLen);
          IFDEBUG IotsaSerial.print(" expLen=");
          IFDEBUG IotsaSerial.print(expDecodeLen);
          IFDEBUG IotsaSerial.print(" b64len=");
          IFDEBUG IotsaSerial.println(b64len);
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("Error base64 decoding httpsCertificate");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsCertificate malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
#endif // IOTSA_WITH_HTTPS
  if( server->hasArg("factoryreset") && server->hasArg("iamsure")) {
    if (server->arg("factoryreset") == "1" && server->arg("iamsure") == "1") {
      iotsaConfig.nextConfigurationMode = IOTSA_MODE_FACTORY_RESET;
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      anyChanged = true;
    }
  }

  if (anyChanged) {
    	configSave();
	}

  String message = "<html><head><title>Iotsa configuration</title></head><body><h1>Iotsa configuration</h1>";
  if (wrongMode) {
    message += "<p><em>Error:</em> must be in configuration mode to change some of these parameters.</p>";
  }
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM.</p>";
    if (hostnameChanged) {
      if (iotsaConfig.wifiPrivateNetworkMode) {
        message += "<p>Not rebooting, so you can also change <a href='/wificonfig'>Wifi config</a>.</p>";
      } else {
        message += "<p><em>Rebooting device to change hostname</em>.</p>";
      }   
    }
    if (iotsaConfig.nextConfigurationMode) {
      message += "<p><em>Special mode ";
      message += modeName(iotsaConfig.nextConfigurationMode);
      message += " requested. Power cycle within ";
      message += String((iotsaConfig.nextConfigurationModeEndTime - millis())/1000);
      message += " seconds to activate.</em></p>";
    }
  }
  if (!iotsaConfig.inConfigurationMode()) {
    message += "<p>Hostname: ";
    message += htmlEncode(iotsaConfig.hostName);
    message += " (goto configuration mode to change)<br>Configuration mode timeout: ";
    message += String(iotsaConfig.configurationModeTimeout);
    message += " (goto configuration mode to change)</p>";
#ifdef IOTSA_WITH_HTTPS
    if (iotsaConfig.httpsKey == defaultHttpsKey) {
      message += "<p>Using factory-installed (<b>not very secure</b>) https certificate</p>";
    } else {
      message += "<p>Using uploaded https certificate.</p>";
    }
#endif // IOTSA_WITH_HTTPS
  }
  message += "<form method='get'>";
  if (iotsaConfig.inConfigurationOrFactoryMode()) {
    message += "Hostname: <input name='hostName' value='";
    message += htmlEncode(iotsaConfig.hostName);
    message += "'><br>";
  }
  if (iotsaConfig.inConfigurationMode()) {
    message += "Configuration mode timeout: <input name='rebootTimeout' value='";
    message += String(iotsaConfig.configurationModeTimeout);
    message += "'><br>";
#ifdef IOTSA_WITH_HTTPS
    message += "HTTPS private key (PEM): <br><textarea name='httpsKey' rows='8' cols='60'></textarea><br>";
    message += "HTTPS certificate (PEM): <br><textarea name='httpsCertificate' rows='8' cols='60'></textarea><br>";
#endif
  }

  message += "<input name='mode' type='radio' value='0' checked> Enter normal mode after next reboot.<br>";
  message += "<input name='mode' type='radio' value='1'> Enter configuration mode after next reboot.<br>";
  if (app.otaEnabled()) {
    message += "<input name='mode' type='radio' value='2'> Enable over-the-air update after next reboot.";
    if (iotsaConfig.wifiPrivateNetworkMode) {
      message += "(<em>Warning:</em> Enabling OTA may not work because mDNS not available on this WiFi network.)";
    }
    message += "<br>";
  }
  message += "<br><input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.<br>";
  message += "<input type='submit'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
  if (hostnameChanged && !iotsaConfig.wifiPrivateNetworkMode) {
    IFDEBUG IotsaSerial.println("Restart in 2 seconds");
    rebootAt = millis() + 2000;
  }
}

String IotsaConfigMod::info() {
  String message;
  if (iotsaConfig.configurationMode) {
  	message += "<p>In configuration mode ";
    message += modeName(iotsaConfig.configurationMode);
    message += ", will timeout in " + String((iotsaConfig.configurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.nextConfigurationMode) {
  	message += "<p>Configuration mode ";
    message += modeName(iotsaConfig.nextConfigurationMode);
    message += " requested, enable by rebooting within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.configurationModeEndTime) {
  	message += "<p>Strange, no configuration mode but timeout is " + String(iotsaConfig.configurationModeEndTime-millis()) + "ms.</p>";
  }
  message += "<p>" + app.title + " is based on iotsa " + IOTSA_FULL_VERSION + ". See <a href=\"/config\">/config</a> to change configuration.";
  message += "Last boot " + String((int)millis()/1000) + " seconds ago, reason ";
  message += getBootReason();
  message += ".</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaConfigMod::getHandler(const char *path, JsonObject& reply) {
  reply["hostName"] = iotsaConfig.hostName;
  reply["modeTimeout"] = iotsaConfig.configurationModeTimeout;
  reply["currentMode"] = int(iotsaConfig.configurationMode);
  if (iotsaConfig.configurationMode) {
    reply["currentModeTimeout"] = (iotsaConfig.configurationModeEndTime - millis())/1000;
  }
  reply["privateWifi"] = iotsaConfig.wifiPrivateNetworkMode;
  reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
  if (iotsaConfig.nextConfigurationMode) {
    reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
  }
  reply["iotsaVersion"] = IOTSA_VERSION;
  reply["iotsaFullVersion"] = IOTSA_FULL_VERSION;
  reply["program"] = app.title;
#ifdef IOTSA_WITH_HTTPS
  reply["defaultCert"] = iotsaConfig.httpsKey == defaultHttpsKey;
  reply["has_httpsKey"] = SPIFFS.exists("/config/httpsKey.der");
  reply["has_httpsCert"] = SPIFFS.exists("/config/httpsCert.der");
#endif
#ifdef IOTSA_CONFIG_PROGRAM_SOURCE
  reply["programSource"] = IOTSA_CONFIG_PROGRAM_SOURCE;
#endif
#ifdef IOTSA_CONFIG_PROGRAM_VERSION
  reply["programVersion"] = IOTSA_CONFIG_PROGRAM_VERSION;
#endif
#ifdef ARDUINO_VARIANT
  reply["board"] = ARDUINO_VARIANT;
#elif defined(ARDUINO_BOARD)
  reply["board"] = ARDUINO_BOARD;
#endif
  reply["bootCause"] = getBootReason();
  reply["uptime"] = millis() / 1000;
  JsonArray modules = reply.createNestedArray("modules");
  for (IotsaBaseMod *m=app.firstEarlyModule; m; m=m->nextModule) {
    if (m->name != "")
      modules.add(m->name);
  }
  for (IotsaBaseMod *m=app.firstModule; m; m=m->nextModule) {
    if (m->name != "")
      modules.add(m->name);
  }
  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool wrongMode = false;
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("hostName")) {
    if (iotsaConfig.inConfigurationOrFactoryMode()) {
      iotsaConfig.hostName = reqObj["hostName"].as<String>();
      anyChanged = true;
      reply["needsReboot"] = true;
    } else {
      wrongMode = true;
    }
  }
  if (reqObj.containsKey("modeTimeout")) {
    if (iotsaConfig.inConfigurationMode()) {
      iotsaConfig.configurationModeTimeout = reqObj["modeTimeout"];
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if (reqObj.containsKey("requestedMode")) {
    iotsaConfig.nextConfigurationMode = config_mode(reqObj["requestedMode"].as<int>());
    anyChanged = iotsaConfig.nextConfigurationMode != config_mode(0);
    if (anyChanged) {
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
      reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
#ifdef IOTSA_WITH_HTTPS
  // Set parameter defaultCert to true to remove any key/certificate
  if (reqObj.containsKey("defaultCert") && reqObj["defaultCert"]) {
    if (iotsaConfig.inConfigurationMode()) {
      SPIFFS.remove("/config/httpsKey.der");
      SPIFFS.remove("/config/httpsCert.der");
    } else {
      wrongMode = true;
    }
  }
  // Allow setting of https key as PEM. Note that using POST with file upload will
  // work more often due to memory constraints and the size of keys and certificates.
  if (reqObj.containsKey("httpsKey")) {
    if (iotsaConfig.inConfigurationMode()) {
      const char *b64Value = reqObj["httpsKey"];
      static const char *head = "-----BEGIN RSA PRIVATE KEY-----";
      static const char *tail = "-----END RSA PRIVATE KEY-----";
      char *headPos = strstr(b64Value, head);
      char *tailPos = strstr(b64Value, tail);
      if (headPos == b64Value && tailPos) {
        b64Value += strlen(head);
        *tailPos = '\0';
      } else {
        IFDEBUG IotsaSerial.println("req httpsKey not PEM");
        b64Value = "";
      }
      int b64len = strlen(b64Value);
      IFDEBUG IotsaSerial.println("req has httpsKey");
      char *tmpValue = (char *)malloc(base64_decode_expected_len(b64len));
      if (tmpValue) {
        int decodedLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decodedLen > 0) {
          newKey = (uint8_t *)tmpValue;
          newKeyLength = decodedLen;
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("could not decode httpsKey");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsKey malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
  // Allow setting of https certificate as PEM. Note that using POST with file upload will
  // work more often due to memory constraints and the size of keys and certificates.
  if (reqObj.containsKey("httpsCertificate")) {
    if (iotsaConfig.inConfigurationMode()) {
      const char *b64Value = reqObj["httpsCertificate"];
      static const char *head = "-----BEGIN CERTIFICATE-----";
      static const char *tail = "-----END CERTIFICATE-----";
      char *headPos = strstr(b64Value, head);
      char *tailPos = strstr(b64Value, tail);
      if (headPos == b64Value && tailPos) {
        b64Value += strlen(head);
        *tailPos = '\0';
      } else {
        IFDEBUG IotsaSerial.println("req httpsCertificate not PEM");
        b64Value = "";
      }
      int b64len = strlen(b64Value);
      IFDEBUG IotsaSerial.println("req has httpsCertificate");
      char *tmpValue = (char *)malloc(base64_decode_expected_len(b64len));
      if (tmpValue) {
        int decodedLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decodedLen > 0) {
          newCertificate = (uint8_t *)tmpValue;
          newCertificateLength = decodedLen;
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("could not decode httpsCertificate");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsCertificate malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
#endif // IOTSA_WITH_HTTPS
  if (wrongMode) {
    IFDEBUG IotsaSerial.println("Not in config mode");
  }
  if (anyChanged) configSave();
  if (reqObj["reboot"]) {
    IFDEBUG IotsaSerial.println("Restart in 2 seconds.");
    rebootAt = millis() + 2000;
    anyChanged = true;
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API
#if defined(IOTSA_WITH_WEB)
static File _uploadFile;
static bool _uploadOK;

void
IotsaConfigMod::uploadHandler() {
  if (needsAuthentication("config")) return;
  HTTPUpload& upload = server->upload();
  _uploadOK = false;
  if(upload.status == UPLOAD_FILE_START){
    if (upload.filename != "httpsKey.der" && upload.filename != "httpsCert.der") {
      IFDEBUG IotsaSerial.println("Incorrect filename");
      return;
    }
    String _uploadfilename = "/config/" + upload.filename;
    IFDEBUG IotsaSerial.print("Uploading ");
    IFDEBUG IotsaSerial.println(_uploadfilename);
    if(SPIFFS.exists(_uploadfilename)) SPIFFS.remove(_uploadfilename);
    _uploadFile = SPIFFS.open(_uploadfilename, "w");
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(_uploadFile) {
        _uploadFile.close();
        _uploadOK = true;
    }
  }
}

void
IotsaConfigMod::uploadOkHandler() {
  String message;
  if (_uploadOK) {
    IFDEBUG IotsaSerial.println("upload ok");
    server->send(200, "text/plain", "OK");
  } else {
    IFDEBUG IotsaSerial.println("upload failed");
    server->send(403, "text/plain", "FAIL");
  }
}
#endif // defined(IOTSA_WITH_WEB) || defined(IOTSA_WITH_API)

void IotsaConfigMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/config", std::bind(&IotsaConfigMod::handler, this));
  server->on("/configupload", HTTP_POST, std::bind(&IotsaConfigMod::uploadOkHandler, this), std::bind(&IotsaConfigMod::uploadHandler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/config", true, true);
  name = "config";
#endif
}

void IotsaConfigMod::configLoad() {
  IotsaSerial.println("xxxjack IotsaConfigMod::configLoad");
  IotsaConfigFileLoad cf("/config/config.cfg");
  int tcm;
  cf.get("mode", tcm, IOTSA_MODE_NORMAL);
  iotsaConfig.configurationMode = (config_mode)tcm;
  cf.get("hostName", iotsaConfig.hostName, "");
  if (iotsaConfig.hostName == "") wifiDefaultHostName();
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

void IotsaConfigMod::configSave() {
  IotsaConfigFileSave cf("/config/config.cfg");
  cf.put("mode", iotsaConfig.nextConfigurationMode); // Note: nextConfigurationMode, which will be read as configurationMode
  cf.put("hostName", iotsaConfig.hostName);
  cf.put("rebootTimeout", iotsaConfig.configurationModeTimeout);
#ifdef IOTSA_WITH_HTTPS
  if (newKey && newCertificate) {
    iotsaConfigFileSaveBinary("/config/httpsKey.der", newKey, newKeyLength);
    IFDEBUG IotsaSerial.println("saved /config/httpsKey.der");
    iotsaConfigFileSaveBinary("/config/httpsCert.der", newCertificate, newCertificateLength);
    IFDEBUG IotsaSerial.println("saved /config/httpsCert.der");
  } else if (newKey || newCertificate) {
    IFDEBUG IotsaSerial.println("Not saving key/cert unless both are set");
  }
#endif // IOTSA_WITH_HTTPS
  IFDEBUG IotsaSerial.println("Saved config.cfg");
}

void IotsaConfigMod::loop() {
  if (rebootAt && millis() > rebootAt) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
  if (iotsaConfig.configurationModeEndTime && millis() > iotsaConfig.configurationModeEndTime) {
    IFDEBUG IotsaSerial.println("Configuration mode timeout. reboot.");
    iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
    iotsaConfig.configurationModeEndTime = 0;
    ESP.restart();
  }
  if (iotsaConfig.nextConfigurationModeEndTime && millis() > iotsaConfig.nextConfigurationModeEndTime) {
    IFDEBUG IotsaSerial.println("Next configuration mode timeout. Clearing.");
    iotsaConfig.nextConfigurationMode = IOTSA_MODE_NORMAL;
    iotsaConfig.nextConfigurationModeEndTime = 0;
    configSave();
  }
}
