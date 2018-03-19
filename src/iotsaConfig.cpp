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

static void wifiDefaultHostName() {
  iotsaConfig.hostName = "iotsa";
#ifdef ESP32
  iotsaConfig.hostName += String(uint32_t(ESP.getEfuseMac()), HEX);
#else
  iotsaConfig.hostName += String(ESP.getChipId(), HEX);
#endif
}

void IotsaConfigMod::setup() {
  // Load configuration parameters, and clear any temporary configuration mode (if requested)
#if 0
  IotsaSerial.println("Enable debug output");
  esp_log_level_set("*", ESP_LOG_DEBUG);
#endif
  iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
  iotsaConfig.nextConfigurationMode = IOTSA_MODE_NORMAL;
  iotsaConfig.configurationModeEndTime = 0;
  iotsaConfig.nextConfigurationModeEndTime = 0;
  privateNetworkModeReason = 0;
  configLoad();
  if (app.status) app.status->showStatus();
  if (iotsaConfig.configurationMode) {
  	IFDEBUG IotsaSerial.println("tmpConfigMode, re-saving wifi.cfg without it");
  	configSave();
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
  bool hotnameChanged = false;
  bool factoryReset = false;
  bool iamsure = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "hostName") {
      iotsaConfig.hostName = server.arg(i);
      anyChanged = true;
      hostnameChanged = true;
    }
    if( server.argName(i) == "rebootTimeout") {
      iotsaConfig.configurationModeTimeout = server.arg(i).toInt();
      anyChanged = true;
    }
    if( server.argName(i) == "config") {
    	if (needsAuthentication("config")) return;
      	iotsaConfig.nextConfigurationMode = config_mode(atoi(server.arg(i).c_str()));
      	iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      	anyChanged = true;
    }
    if( server.argName(i) == "factoryreset" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: factoryReset does NOT require authenticationso users have a way to reclaim
    	// hardware for which they have lost the username/password. The device will, however,
    	// be reset to factory settings, so no information can be leaked.
    	factoryReset = true;
    	anyChanged = true;
  	}
    if( server.argName(i) == "iamsure" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: does not set anyChanged, so only has a function if factoryReset is also set
    	iamsure = true;
  	}
  }
  if (factoryReset && iamsure) {
  	iotsaConfig.nextConfigurationMode = IOTSA_MODE_FACTORY_RESET;
  	iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
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
      String reqMode = "???";
      if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_CONFIG)
        reqMode = "configuration";
      else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_OTA)
        reqMode = "OTA";
      else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_FACTORY_RESET)
        reqMode = "factory-reset";
      message += "<p><em>Special mode "+reqMode+" requested. Power cycle within ";
      message += String((iotsaConfig.nextConfigurationModeEndTime - millis()/1000);
      message += " seconds to activate.</em></p>";
    }
  }
  if (!haveMDNS && app.otaEnabled() {
  	message += "<p>(<em>Warning:</em> Enabling OTA may not work because mDNS not available on this WiFi network.)</p>";
  }
  message += "<form method='get'>";
  if (inConfigurationMode) {
    message += "Hostname: <input name='hostName' value='";
    message += htmlEncode(iotsaConfig.hostName);
    message += "'><br>Configuration mode timeout: <input name='rebootTimeout' value='";
    message += String(iotsaConfig.configurationModeTimeout);
    message += "><br>";
  } else {
    message += "Hostname: ";
    message += htmlEncode(iotsaConfig.hostName);
  }
  message += "'><br><input type='submit'></form>";
  message += "</body></html>";
  server.send(200, "text/html", message);
  if (hostnameChanged) {
    IFDEBUG IotsaSerial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

void
IotsaConfigMod::handlerNormalMode() {
  bool anyChanged = false;
  bool factoryReset = false;
  bool iamsure = false;
  for (uint8_t i=0; i<server.args(); i++) {
    if( server.argName(i) == "config") {
    	if (needsAuthentication("config")) return;
      	iotsaConfig.nextConfigurationMode = config_mode(atoi(server.arg(i).c_str()));
      	iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      	anyChanged = true;
    }
    if( server.argName(i) == "factoryreset" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: factoryReset does NOT require authenticationso users have a way to reclaim
    	// hardware for which they have lost the username/password. The device will, however,
    	// be reset to factory settings, so no information can be leaked.
    	factoryReset = true;
    	anyChanged = true;
	}
    if( server.argName(i) == "iamsure" && atoi(server.arg(i).c_str()) == 1) {
    	// Note: does not set anyChanged, so only has a function if factoryReset is also set
    	iamsure = true;
	}
  }
  if (factoryReset && iamsure) {
  	iotsaConfig.nextConfigurationMode = IOTSA_MODE_FACTORY_RESET;
	iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
  }
  if (anyChanged) {
	if (app.status) app.status->showStatus();
	configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_CONFIG) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + " seconds to activate configuration mode for " + String(iotsaConfig.configurationModeTimeout) + " seconds.</em></p>";
  	message += "<p><em>Connect to WiFi network config-" + htmlEncode(iotsaConfig.hostName) + ", device 192.168.4.1 to change settings during that configuration period. </em></p>";
  } else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_OTA) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + "seconds to activate OTA mode for " + String(iotsaConfig.configurationModeTimeout) + " seconds.</em></p>";
  } else if (iotsaConfig.nextConfigurationMode == IOTSA_MODE_FACTORY_RESET) {
  	message += "<p><em>Power-cycle device within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + "seconds to reset to factory settings.</em></p>";
  }
  message += "<form method='get'><input name='config' type='checkbox' value='1'> Enter configuration mode after next reboot.<br>";
  if (app.otaEnabled()) {
	  message += "<input name='config' type='checkbox' value='2'> Enable over-the-air update after next reboot.</br>";
  }
  message += "<input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.</br>";
  message += "<br><input type='submit'></form></body></html>";
  server.send(200, "text/html", message);
}

bool IotsaConfigMod::getHandler(const char *path, JsonObject& reply) {
  reply["hostName"] = iotsaConfig.hostName;
  reply["rebootTimeout"] = iotsaConfig.configurationModeTimeout;
  if (iotsaConfig.wifiPrivateNetworkMode) {
    reply["ssid"] = ssid;
    reply["ssidPassword"] = ssidPassword;
  } else {
    if (iotsaConfig.nextConfigurationMode) {
      reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
      reply["timeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
    }
  }
  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (iotsaConfig.wifiPrivateNetworkMode) {
    if (reqObj.containsKey("ssid")) {
      ssid = reqObj.get<String>("ssid");
      anyChanged = true;
    }
    if (reqObj.containsKey("ssidPassword")) {
      ssid = reqObj.get<String>("ssidPassword");
      anyChanged = true;
    }
    if (reqObj.containsKey("hostName")) {
      ssid = reqObj.get<String>("hostName");
      anyChanged = true;
    }
    if (reqObj.containsKey("rebootTimeout")) {
      ssid = reqObj.get<int>("rebootTimeout");
      anyChanged = true;
    }
  } else {
    if (reqObj.containsKey("requestedMode")) {
      iotsaConfig.nextConfigurationMode = config_mode(reqObj.get<int>("requestedMode"));
      anyChanged = iotsaConfig.nextConfigurationMode != config_mode(0);
      if (anyChanged) {
        iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
        reply["requestedMode"] = iotsaConfig.nextConfigurationMode;
        reply["timeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      }
    }
  }
  if (anyChanged) configSave();
  return anyChanged;
}

void IotsaConfigMod::serverSetup() {
//  server.on("/hello", std::bind(&IotsaConfigMod::handler, this));
  if (iotsaConfig.wifiPrivateNetworkMode) {
    server.on("/wificonfig", std::bind(&IotsaConfigMod::handlerConfigMode, this));
  } else {
    server.on("/wificonfig", std::bind(&IotsaConfigMod::handlerNormalMode, this));
  }
  api.setup("/api/wificonfig", true, true);
}

String IotsaConfigMod::info() {
  IPAddress x;
  String message = "<p>IP address is ";
  uint32_t ip = WiFi.localIP();
  if (ip == 0) {
  	ip = WiFi.softAPIP();
  }
  message += String(ip&0xff) + "." + String((ip>>8)&0xff) + "." + String((ip>>16)&0xff) + "." + String((ip>>24)&0xff);
  if (haveMDNS) {
    message += ", hostname is ";
    message += htmlEncode(iotsaConfig.hostName);
    message += ".local. ";
  } else {
    message += " (no mDNS, so no hostname). ";
  }
  message += "See <a href=\"/wificonfig\">/wificonfig</a> to change network parameters.</p>";
  if (iotsaConfig.configurationMode) {
  	message += "<p>In configuration mode " + String((int)iotsaConfig.configurationMode) + "(reason: " + String(privateNetworkModeReason) + "), will timeout in " + String((iotsaConfig.configurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.nextConfigurationMode) {
  	message += "<p>Configuration mode " + String((int)iotsaConfig.nextConfigurationMode) + " requested, enable by rebooting within " + String((iotsaConfig.nextConfigurationModeEndTime-millis())/1000) + " seconds.</p>";
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
  return message;
}

void IotsaConfigMod::configLoad() {
  IotsaConfigFileLoad cf("/config/wifi.cfg");
  int tcm;
  cf.get("mode", tcm, (int)IOTSA_MODE_NORMAL);
  iotsaConfig.configurationMode = (config_mode)tcm;
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
  cf.get("hostName", iotsaConfig.hostName, "");
  cf.get("rebootTimeout", iotsaConfig.configurationModeTimeout, CONFIGURATION_MODE_TIMEOUT);
  if (iotsaConfig.hostName == "") wifiDefaultHostName();
 
}

void IotsaConfigMod::configSave() {
  IotsaConfigFileSave cf("/config/wifi.cfg");
  cf.put("mode", iotsaConfig.nextConfigurationMode); // Note: nextConfigurationMode, which will be read as configurationMode
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  cf.put("hostName", iotsaConfig.hostName);
  cf.put("rebootTimeout", iotsaConfig.configurationModeTimeout);
  IFDEBUG IotsaSerial.println("Saved wifi.cfg");
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
#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (haveMDNS) MDNS.update();
#endif
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
