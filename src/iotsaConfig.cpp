#include <ESP.h>
#include <user_interface.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <esp_log.h>
#else
#include <ESP8266mDNS.h>
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

String& hostName(iotsaConfig.hostName);

static void wifiDefaultHostName() {
  iotsaConfig.hostName = "iotsa";
#ifdef ESP32
  iotsaConfig.hostName += String(uint32_t(ESP.getEfuseMac()), HEX);
#else
  iotsaConfig.hostName += String(ESP.getChipId(), HEX);
#endif
}

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

void IotsaConfigMod::setup() {
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
  rst_info *rip = ESP.getResetInfoPtr();
  if (rip->reason != REASON_DEFAULT_RST && rip->reason != REASON_EXT_SYS_RST) {
    iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
    IFDEBUG IotsaSerial.println("tmpConfigMode not honoured because of reset reason");
  }
  // If factory reset is requested format the Flash and reboot
  if (iotsaConfig.configurationMode == IOTSA_MODE_FACTORY_RESET) {
  	IFDEBUG IotsaSerial.println("Factory-reset requested");
  	delay(1000);
#ifndef ESP32
  	IFDEBUG IotsaSerial.println("Formatting SPIFFS...");
  	SPIFFS.format();
  	IFDEBUG IotsaSerial.println("Format done, rebooting.");
#else
	SPIFFS.remove("/config/wifi.cfg");
	IFDEBUG IotsaSerial.println("Removed /config/wifi.cfg");
#endif
  	delay(2000);
  	ESP.restart();
  }
  if (app.status) app.status->showStatus();
}

void
IotsaConfigMod::handler() {
  bool anyChanged = false;
  bool hostnameChanged = false;
  if( server.hasArg("hostName")) {
    String argValue = server.arg("hostname");
    if (argValue != iotsaConfig.hostName) {
      iotsaConfig.hostName = argValue;
      anyChanged = true;
      hostnameChanged = true;
    }
  }
  if( server.hasArg("rebootTimeout")) {
    int newValue = server.arg("rebootTimeout").toInt();
    if (newValue != iotsaConfig.configurationModeTimeout) {
      iotsaConfig.configurationModeTimeout = newValue;
      anyChanged = true;
    }
  }
  if( server.hasArg("mode")) {
    String argValue = server.arg("mode");
    if (argValue != "0") {
      if (needsAuthentication("config")) return;
      iotsaConfig.nextConfigurationMode = config_mode(atoi(argValue.c_str()));
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      anyChanged = true;
    }
  }
  if( server.hasArg("factoryreset") && server.hasArg("iamsure")) {
    if (server.arg("factoryreset") == "1" && server.arg("iamsure") == "1") {
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
  server.send(200, "text/html", message);
  if (hostnameChanged) {
    IFDEBUG IotsaSerial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

bool IotsaConfigMod::getHandler(const char *path, JsonObject& reply) {
  reply["hostName"] = iotsaConfig.hostName;
  reply["modeTimeout"] = iotsaConfig.configurationModeTimeout;
  if (iotsaConfig.configurationMode) {
    reply["currentMode"] = iotsaConfig.configurationMode;
    reply["currentModeTimeout"] = (iotsaConfig.configurationModeEndTime - millis())/1000;
  }
  if (iotsaConfig.nextConfigurationMode) {
    reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
    reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
  }
  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool hostnameChanged = true;
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("hostName")) {
    iotsaConfig.hostName = reqObj.get<String>("hostName");
    anyChanged = true;
    hostnameChanged = true;
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
      reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
      reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  if (anyChanged) configSave();
  if (reqObj.get<bool>("reboot")) {
    delay(2000);
    ESP.restart();
  }
  return anyChanged;
}

void IotsaConfigMod::serverSetup() {
  server.on("/config", std::bind(&IotsaConfigMod::handler, this));
  api.setup("/api/config", true, true);
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
  message += "<p>Last boot " + String((int)millis()/1000) + " seconds ago, reason ";
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
  const char *reason = "unknown";
  if ((int)rip->reason < sizeof(reasons)/sizeof(reasons[0])) {
    reason = reasons[(int)rip->reason];
  }
  message += reason;
  message += " ("+String((int)rip->reason)+").</p>";
  message += "<p>See <a href=\"/config\">/config</a> to change configuration.</p>";
  return message;
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
