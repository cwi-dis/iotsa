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

//
// Global variables, because other modules need them too.
//
IotsaConfig iotsaConfig = {
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

  0x30, 0x82, 0x01, 0xc3, 0x30, 0x82, 0x01, 0x2c, 0x02, 0x09, 0x00, 0xbb,
  0xee, 0xff, 0x9f, 0x86, 0x5d, 0x42, 0x6a, 0x30, 0x0d, 0x06, 0x09, 0x2a,
  0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x26,
  0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x05, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0b, 0x31, 0x39, 0x32, 0x2e, 0x31, 0x36, 0x38, 0x2e, 0x34,
  0x2e, 0x31, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x38, 0x30, 0x35, 0x32, 0x35,
  0x32, 0x32, 0x34, 0x30, 0x35, 0x38, 0x5a, 0x17, 0x0d, 0x33, 0x32, 0x30,
  0x32, 0x30, 0x31, 0x32, 0x32, 0x34, 0x30, 0x35, 0x38, 0x5a, 0x30, 0x26,
  0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x05, 0x69,
  0x6f, 0x74, 0x73, 0x61, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x0c, 0x0b, 0x31, 0x39, 0x32, 0x2e, 0x31, 0x36, 0x38, 0x2e, 0x34,
  0x2e, 0x31, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
  0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00,
  0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xc3, 0x98, 0x57, 0x55, 0xf8,
  0x0c, 0x15, 0xae, 0xe6, 0x93, 0x40, 0xfd, 0x3f, 0x30, 0xd4, 0xf3, 0x90,
  0xe9, 0xa8, 0x6a, 0x6c, 0xe0, 0xbf, 0xe0, 0x9e, 0x41, 0x30, 0x8e, 0xd2,
  0x2f, 0xba, 0xd5, 0x2c, 0x68, 0xe3, 0xbf, 0xd2, 0x50, 0xca, 0x41, 0xe2,
  0x30, 0x2f, 0xfb, 0x61, 0xfa, 0xb0, 0x07, 0xc4, 0x2c, 0xd0, 0xb5, 0xcc,
  0x7e, 0xb7, 0x4c, 0xcd, 0x3a, 0xf0, 0x3d, 0x71, 0xe4, 0xf6, 0xdd, 0x2e,
  0xa5, 0xc7, 0xe5, 0xe5, 0xf4, 0xa1, 0x1d, 0x3b, 0x64, 0x6c, 0xd3, 0xb2,
  0x3d, 0x86, 0x3a, 0x35, 0xca, 0x19, 0x93, 0x04, 0x4b, 0xc7, 0x02, 0x31,
  0x13, 0x51, 0xb9, 0x60, 0xba, 0xf9, 0x5a, 0x62, 0xd6, 0x77, 0x0f, 0x2d,
  0x3b, 0xed, 0xb4, 0x47, 0x3b, 0x8a, 0xee, 0x5b, 0x92, 0x62, 0xee, 0xee,
  0xf6, 0xbb, 0x21, 0x27, 0xd0, 0x4c, 0x8f, 0x3c, 0x38, 0x96, 0x6f, 0x84,
  0x64, 0xd3, 0x81, 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x0d, 0x06, 0x09,
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03,
  0x81, 0x81, 0x00, 0x06, 0x70, 0x67, 0xb0, 0x50, 0xd4, 0x8f, 0x49, 0x8c,
  0x03, 0x0a, 0xd4, 0x45, 0xb7, 0x12, 0xb9, 0xaf, 0x53, 0x48, 0x28, 0x4f,
  0x12, 0x92, 0x41, 0x17, 0xc6, 0xd2, 0xd2, 0xb8, 0x18, 0xbb, 0x7f, 0xee,
  0x0b, 0x5e, 0x01, 0x23, 0x2e, 0xf8, 0x74, 0x06, 0xfe, 0x7c, 0xd0, 0x2c,
  0x6b, 0x5d, 0x0a, 0xc1, 0xc5, 0xeb, 0xc9, 0x54, 0x8f, 0x2f, 0x2c, 0x6b,
  0xc3, 0x20, 0x94, 0x1c, 0x42, 0xb3, 0x20, 0xae, 0x0b, 0xf2, 0xd5, 0x65,
  0xd3, 0xac, 0x6b, 0x7c, 0x6c, 0x90, 0x4f, 0x81, 0x10, 0x6d, 0x1a, 0x93,
  0xa9, 0xcc, 0x55, 0x73, 0x95, 0x4e, 0x67, 0xb3, 0x20, 0xf7, 0x6c, 0x63,
  0x4f, 0xa8, 0xc4, 0x87, 0x10, 0x09, 0x30, 0x98, 0x11, 0x6d, 0x5a, 0x57,
  0xae, 0x4b, 0xd1, 0x01, 0xb3, 0xb6, 0xee, 0xfd, 0x82, 0xcf, 0x0f, 0xd3,
  0x39, 0x6c, 0x2a, 0xf7, 0xc6, 0x69, 0xb2, 0xe9, 0x55, 0x4f, 0x92
};

// And so is the key.  These could also be in DRAM
static const uint8_t defaultHttpsKey[] PROGMEM = {

  0x30, 0x82, 0x02, 0x5b, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xc3,
  0x98, 0x57, 0x55, 0xf8, 0x0c, 0x15, 0xae, 0xe6, 0x93, 0x40, 0xfd, 0x3f,
  0x30, 0xd4, 0xf3, 0x90, 0xe9, 0xa8, 0x6a, 0x6c, 0xe0, 0xbf, 0xe0, 0x9e,
  0x41, 0x30, 0x8e, 0xd2, 0x2f, 0xba, 0xd5, 0x2c, 0x68, 0xe3, 0xbf, 0xd2,
  0x50, 0xca, 0x41, 0xe2, 0x30, 0x2f, 0xfb, 0x61, 0xfa, 0xb0, 0x07, 0xc4,
  0x2c, 0xd0, 0xb5, 0xcc, 0x7e, 0xb7, 0x4c, 0xcd, 0x3a, 0xf0, 0x3d, 0x71,
  0xe4, 0xf6, 0xdd, 0x2e, 0xa5, 0xc7, 0xe5, 0xe5, 0xf4, 0xa1, 0x1d, 0x3b,
  0x64, 0x6c, 0xd3, 0xb2, 0x3d, 0x86, 0x3a, 0x35, 0xca, 0x19, 0x93, 0x04,
  0x4b, 0xc7, 0x02, 0x31, 0x13, 0x51, 0xb9, 0x60, 0xba, 0xf9, 0x5a, 0x62,
  0xd6, 0x77, 0x0f, 0x2d, 0x3b, 0xed, 0xb4, 0x47, 0x3b, 0x8a, 0xee, 0x5b,
  0x92, 0x62, 0xee, 0xee, 0xf6, 0xbb, 0x21, 0x27, 0xd0, 0x4c, 0x8f, 0x3c,
  0x38, 0x96, 0x6f, 0x84, 0x64, 0xd3, 0x81, 0x02, 0x03, 0x01, 0x00, 0x01,
  0x02, 0x81, 0x80, 0x36, 0xc0, 0x29, 0x0a, 0x56, 0x81, 0xc3, 0x7c, 0x0e,
  0xec, 0xff, 0x4f, 0x24, 0x66, 0x1d, 0xe6, 0x04, 0x15, 0x73, 0xe0, 0x3e,
  0x93, 0xf7, 0x02, 0x00, 0x2a, 0x8d, 0x56, 0x1d, 0x3d, 0xe1, 0x15, 0x94,
  0xf5, 0xd3, 0x72, 0xb9, 0x83, 0x85, 0xea, 0x45, 0x4f, 0x69, 0xce, 0xfb,
  0x51, 0x39, 0xff, 0x22, 0x89, 0xcc, 0xee, 0x66, 0xcc, 0xbd, 0xb0, 0x90,
  0xee, 0x43, 0x9b, 0x5f, 0x8b, 0x51, 0x12, 0x81, 0x05, 0x75, 0x9a, 0x6e,
  0x5f, 0xa5, 0x72, 0xc1, 0xa4, 0x97, 0x58, 0x12, 0xa8, 0x52, 0x52, 0xc8,
  0xa9, 0x1c, 0x2e, 0xae, 0x09, 0x38, 0xed, 0xfd, 0x84, 0xe8, 0x7f, 0xf2,
  0x22, 0x27, 0xd8, 0xf2, 0x85, 0xaa, 0xd7, 0x01, 0x88, 0x13, 0x5d, 0x4e,
  0x38, 0x73, 0x0f, 0x8d, 0x7d, 0x82, 0x0f, 0xf0, 0x34, 0xbb, 0xb8, 0xe7,
  0x36, 0xb5, 0x76, 0xda, 0xfc, 0xd3, 0x5b, 0x32, 0x18, 0x6f, 0xb1, 0x02,
  0x41, 0x00, 0xf6, 0x6c, 0x1d, 0x04, 0xfe, 0x07, 0x34, 0x33, 0xf7, 0x18,
  0x65, 0x0a, 0x1b, 0x61, 0x4e, 0xc4, 0x9c, 0xc0, 0xc6, 0x9c, 0xac, 0x2e,
  0x78, 0x85, 0xb5, 0x3e, 0xcd, 0xae, 0xbd, 0x94, 0xc4, 0x53, 0x7e, 0x24,
  0x15, 0xe7, 0xca, 0x73, 0x2c, 0x3a, 0x58, 0x06, 0xb1, 0xaf, 0x47, 0x19,
  0xe5, 0x9b, 0x38, 0xb7, 0xcf, 0xb1, 0x7e, 0x4d, 0x64, 0xc6, 0xad, 0x8f,
  0xb0, 0xa0, 0x7b, 0x4c, 0x2f, 0x6d, 0x02, 0x41, 0x00, 0xcb, 0x32, 0x7f,
  0xff, 0x42, 0x04, 0xad, 0x31, 0xf4, 0xb1, 0xb8, 0x0a, 0x6c, 0x4e, 0x81,
  0xa0, 0x9e, 0xdb, 0x66, 0xb2, 0x7e, 0x3d, 0x80, 0x2d, 0x1a, 0x50, 0xd3,
  0x11, 0xc2, 0x64, 0x7c, 0xcb, 0x78, 0x05, 0xc4, 0xa0, 0xbe, 0xc5, 0xd7,
  0x36, 0x66, 0x30, 0x27, 0xe8, 0xca, 0x3e, 0x95, 0x64, 0x89, 0x9f, 0x80,
  0x64, 0xf2, 0x1f, 0x94, 0x77, 0x35, 0x6c, 0x6d, 0x4b, 0x8f, 0xbc, 0xa3,
  0xe5, 0x02, 0x40, 0x3e, 0xf6, 0x46, 0xbf, 0xe4, 0xcc, 0x20, 0x69, 0x7a,
  0xa4, 0x10, 0x04, 0xf2, 0x13, 0xfd, 0xd5, 0x3c, 0x9c, 0x00, 0xe3, 0x3d,
  0x17, 0x2e, 0x92, 0x33, 0x4a, 0x15, 0xb1, 0xa5, 0x1c, 0xe2, 0xc0, 0xd6,
  0x85, 0x0f, 0xd7, 0xc6, 0xa1, 0x80, 0xd6, 0x73, 0x71, 0x5a, 0x6b, 0x07,
  0x86, 0xb5, 0x64, 0xe0, 0xac, 0x0e, 0x74, 0x32, 0x6a, 0x41, 0xea, 0x85,
  0xa4, 0x26, 0x24, 0x0a, 0xfc, 0xdf, 0x4d, 0x02, 0x40, 0x5f, 0x50, 0xc3,
  0x05, 0xe3, 0xdb, 0xf9, 0xba, 0x53, 0x44, 0x02, 0x46, 0xb1, 0x63, 0x6a,
  0x1f, 0x04, 0x25, 0x7a, 0xd9, 0x03, 0xaa, 0xa9, 0xb3, 0x7e, 0x82, 0xa7,
  0x5f, 0xcf, 0x45, 0xff, 0xdc, 0x19, 0xe2, 0xea, 0xc7, 0x54, 0x75, 0xcd,
  0x6c, 0x31, 0x27, 0x29, 0xb1, 0x63, 0x1d, 0x54, 0x4a, 0xa2, 0xdb, 0xf5,
  0x08, 0x65, 0x23, 0x37, 0x2b, 0x6c, 0x1d, 0xfe, 0x6e, 0xd1, 0x6f, 0xf5,
  0x99, 0x02, 0x40, 0x0f, 0x7a, 0x65, 0xd0, 0xdb, 0x09, 0x08, 0xf4, 0x6a,
  0xf4, 0xd7, 0xc2, 0xed, 0xb8, 0x05, 0x47, 0x6d, 0x8a, 0x16, 0xf6, 0x97,
  0x4e, 0x06, 0x7b, 0xf6, 0xf6, 0x31, 0x75, 0x2f, 0xf8, 0x5c, 0x35, 0xcd,
  0xf2, 0xb6, 0x95, 0xdb, 0xbf, 0x38, 0x3d, 0xd8, 0x4a, 0x34, 0x6b, 0xa1,
  0xf1, 0x1e, 0xed, 0x19, 0x9a, 0x4d, 0x9a, 0xb8, 0xb8, 0x39, 0xef, 0xd3,
  0x8a, 0xb0, 0xef, 0xa9, 0x1e, 0x16, 0xf5
};
#endif

String& hostName(iotsaConfig.hostName);

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
#ifdef IOTSA_WITH_HTTPS
  iotsaConfig.httpsCertificate = defaultHttpsCertificate;
  iotsaConfig.httpsCertificateLength = sizeof(defaultHttpsCertificate);
  iotsaConfig.httpsKey = defaultHttpsKey;
  iotsaConfig.httpsKeyLength = sizeof(defaultHttpsKey);  
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
  bool anyChanged = false;
  bool hostnameChanged = false;
  if( server->hasArg("hostName")) {
    String argValue = server->arg("hostname");
    if (argValue != iotsaConfig.hostName) {
      iotsaConfig.hostName = argValue;
      anyChanged = true;
      hostnameChanged = true;
    }
  }
  if( server->hasArg("rebootTimeout")) {
    int newValue = server->arg("rebootTimeout").toInt();
    if (newValue != iotsaConfig.configurationModeTimeout) {
      iotsaConfig.configurationModeTimeout = newValue;
      anyChanged = true;
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
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM.</p>";
    if (hostnameChanged) {
      message += "<p><em>Rebooting device to change hostname</em>.</p>";    
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
  }
  message += "<form method='get'>";
  if (iotsaConfig.inConfigurationMode()) {
    message += "Hostname: <input name='hostName' value='";
    message += htmlEncode(iotsaConfig.hostName);
    message += "'><br>Configuration mode timeout: <input name='rebootTimeout' value='";
    message += String(iotsaConfig.configurationModeTimeout);
    message += "'><br>";
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
  message += "<br><input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.</br>";
  message += "<input type='submit'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
  if (hostnameChanged) {
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
  if (iotsaConfig.configurationMode) {
    reply["currentMode"] = int(iotsaConfig.configurationMode);
    reply["currentModeTimeout"] = (iotsaConfig.configurationModeEndTime - millis())/1000;
  }
  if (iotsaConfig.wifiPrivateNetworkMode) {
    reply["privateWifi"] = true;
  }
  if (iotsaConfig.nextConfigurationMode) {
    reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
    reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
  }
  reply["iotsaVersion"] = IOTSA_VERSION;
  reply["iotsaFullVersion"] = IOTSA_FULL_VERSION;
  reply["program"] = app.title;
#ifdef IOTSA_CONFIG_PROGRAM_SOURCE
  reply["programSource"] = IOTSA_CONFIG_PROGRAM_SOURCE;
#endif
#ifdef IOTSA_CONFIG_PROGRAM_VERSION
  reply["programVersion"] = IOTSA_CONFIG_PROGRAM_VERSION;
#endif
#ifdef ARDUINO_VARIANT
  reply["board"] = ARDUINO_VARIANT;
#endif
  reply["bootCause"] = getBootReason();
  reply["uptime"] = millis() / 1000;
  JsonArray& modules = reply.createNestedArray("modules");
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
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("hostName")) {
    iotsaConfig.hostName = reqObj.get<String>("hostName");
    anyChanged = true;
    reply["needsReboot"] = true;
  }
  if (reqObj.containsKey("modeTimeout")) {
    iotsaConfig.configurationModeTimeout = reqObj.get<int>("modeTimeout");
    anyChanged = true;
  }
  if (reqObj.containsKey("requestedMode")) {
    iotsaConfig.nextConfigurationMode = config_mode(reqObj.get<int>("requestedMode"));
    anyChanged = iotsaConfig.nextConfigurationMode != config_mode(0);
    if (anyChanged) {
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
      reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  if (anyChanged) configSave();
  if (reqObj.get<bool>("reboot")) {
    IFDEBUG IotsaSerial.println("Restart in 2 seconds.");
    rebootAt = millis() + 2000;
    anyChanged = true;
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaConfigMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/config", std::bind(&IotsaConfigMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/config", true, true);
  name = "config";
#endif
}

void IotsaConfigMod::configLoad() {
  IotsaConfigFileLoad cf("/config/config.cfg");
  int tcm;
  cf.get("mode", tcm, -1);
  if (tcm == -1) {
    // Fallback: get from wifi.cfg
    IotsaConfigFileLoad cfcompat("/config/wifi.cfg");
    cfcompat.get("mode", tcm, IOTSA_MODE_NORMAL);
    iotsaConfig.configurationMode = (config_mode)tcm;
    cfcompat.get("hostName", iotsaConfig.hostName, "");
    cfcompat.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
    if (iotsaConfig.hostName == "") wifiDefaultHostName();
    return;
  }
  iotsaConfig.configurationMode = (config_mode)tcm;
  cf.get("hostName", iotsaConfig.hostName, "");
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
  if (iotsaConfig.hostName == "") wifiDefaultHostName();
 
}

void IotsaConfigMod::configSave() {
  IotsaConfigFileSave cf("/config/config.cfg");
  cf.put("mode", iotsaConfig.nextConfigurationMode); // Note: nextConfigurationMode, which will be read as configurationMode
  cf.put("hostName", iotsaConfig.hostName);
  cf.put("rebootTimeout", iotsaConfig.configurationModeTimeout);
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
  // xxxjack
  if (!iotsaConfig.wifiPrivateNetworkMode) {
  	// Should be in normal mode, check that we have WiFi
  	static int disconnectedCount = 0;
  	if (WiFi.status() == WL_CONNECTED) {
  		if (disconnectedCount) {
  			IFDEBUG IotsaSerial.println("Config reconnected");
  		}
  		disconnectedCount = 0;
	} else {
		if (disconnectedCount == 0) {
			IFDEBUG IotsaSerial.println("Config connection lost");
		}
		disconnectedCount++;
		if (disconnectedCount > 60000) {
			IFDEBUG IotsaSerial.println("Config connection lost too long. Reboot.");
			ESP.restart();
		}
	}
  }
}
