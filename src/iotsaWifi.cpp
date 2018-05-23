#include <Esp.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <esp_log.h>
#else
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif
#include <FS.h>

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaWifi.h"

#ifdef IOTSA_WITH_WIFI

static int privateNetworkModeReason;
static unsigned long rebootAt;

void IotsaWifiMod::setup() {
  configLoad();
  if (app.status) app.status->showStatus();

  // Try and connect to an existing Wifi network, if known
  if (ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), ssidPassword.c_str());
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    IFDEBUG IotsaSerial.println("");
  
    // Wait for connection
    int count = IOTSA_WIFI_TIMEOUT;
    while (WiFi.status() != WL_CONNECTED && count > 0) {
      delay(1000);
      IFDEBUG IotsaSerial.print(".");
      count--;
    }
    if (count) {
      // Connection to WiFi network succeeded.
      IFDEBUG IotsaSerial.println("");
      IFDEBUG IotsaSerial.print("Connected to ");
      IFDEBUG IotsaSerial.println(ssid);
      IFDEBUG IotsaSerial.print("IP address: ");
      IFDEBUG IotsaSerial.println(WiFi.localIP());
      IFDEBUG IotsaSerial.print("Hostname ");
      IFDEBUG IotsaSerial.println(iotsaConfig.hostName);
      
      WiFi.setAutoReconnect(true);

      if (MDNS.begin(iotsaConfig.hostName.c_str())) {
#ifdef ESP32
        MDNS.addService("_http", "_tcp", 80);
        MDNS.addService("_iotsa", "_tcp", 80);
#else
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("iotsa", "tcp", 80);
#endif
        IFDEBUG IotsaSerial.println("MDNS responder started");
        haveMDNS = true;
      }
      if (app.status) app.status->showStatus();
      return;
    }
    iotsaConfig.wifiPrivateNetworkMode = true;
    privateNetworkModeReason = WiFi.status();
    iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
    IFDEBUG IotsaSerial.print("Cannot join ");
    IFDEBUG IotsaSerial.print(ssid);
    IFDEBUG IotsaSerial.print(", status=");
    IFDEBUG IotsaSerial.println(privateNetworkModeReason);
    IFDEBUG IotsaSerial.print(", retry at ");
    IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
  }
  if (app.status) app.status->showStatus();
  
  iotsaConfig.wifiPrivateNetworkMode = true; // xxxjack
  String networkName = "config-" + iotsaConfig.hostName;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(networkName.c_str());
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  IFDEBUG IotsaSerial.print("\nCreating softAP for network ");
  IFDEBUG IotsaSerial.println(networkName);
  IFDEBUG IotsaSerial.print("IP address: ");
  IFDEBUG IotsaSerial.println(WiFi.softAPIP());
#if 0
  // Despite reports to the contrary it seems mDNS isn't working in softAP mode
  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    IFDEBUG IotsaSerial.println("MDNS responder started");
    haveMDNS = true;
  }
#endif
  if (app.status) app.status->showStatus();
}

#ifdef IOTSA_WITH_WEB
void
IotsaWifiMod::handler() {
  bool wrongMode = false;
  if (needsAuthentication("config")) return;
  bool anyChanged = false;
  if( server->hasArg("ssid")) {
    if (iotsaConfig.inConfigurationMode() || iotsaConfig.wifiPrivateNetworkMode) {
      ssid = server->arg("ssid");
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if( server->hasArg("ssidPassword")) {
    if (iotsaConfig.inConfigurationMode() || iotsaConfig.wifiPrivateNetworkMode) {
      ssidPassword = server->arg("ssidPassword");
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if (anyChanged) {
    configSave();
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
  if (wrongMode) {
    message += "<p><em>Error:</em> must be in configuration mode to change WiFi settings. See <a href='/config'>/config</a> to enable.</p>";
  }
  message += "<p>Hostname: ";
  message += htmlEncode(iotsaConfig.hostName);
  message += ", see <a href='/config'>/config</a> to change.</p>";
  message += "<form method='get'>Network: <input name='ssid' value='";
  message += htmlEncode(ssid);
  message += "'><br>Password: <input type='password' name='ssidPassword'><br><input type='submit'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
  if (anyChanged) {
    if (app.status) app.status->showStatus();
    IFDEBUG IotsaSerial.println("Restart in 2 seconds.");
    rebootAt = millis() + 2000;
  }
}

String IotsaWifiMod::info() {
  IPAddress x;
  String message = "<p>IP address is ";
  uint32_t ip = WiFi.localIP();
  if (ip == 0) {
  	ip = WiFi.softAPIP();
  }
  message += String(ip&0xff) + "." + String((ip>>8)&0xff) + "." + String((ip>>16)&0xff) + "." + String((ip>>24)&0xff);
  message += ", hostname is ";
  message += htmlEncode(iotsaConfig.hostName);
  message += ".local. ";
  if (iotsaConfig.wifiPrivateNetworkMode) {
    message += " (but no mDNS on this WiFi network, so using hostname will not work). ";
  }
  message += "See <a href=\"/wificonfig\">/wificonfig</a> to change network parameters.</p>";

  message += "</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaWifiMod::getHandler(const char *path, JsonObject& reply) {
  reply["ssid"] = ssid;
#if 0
  reply["ssidPassword"] = ssidPassword;
#endif
  return true;
}

bool IotsaWifiMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("ssid")) {
    ssid = reqObj.get<String>("ssid");
    anyChanged = true;
  }
  if (reqObj.containsKey("ssidPassword")) {
    ssidPassword = reqObj.get<String>("ssidPassword");
    anyChanged = true;
  }
  if (anyChanged) configSave();
  if (reqObj.get<bool>("reboot")) {
    IFDEBUG IotsaSerial.println("Restart in 2 seconds.");
    rebootAt = millis() + 2000;
  }
  return anyChanged;
}
#endif // IOTSA_WITH_API

void IotsaWifiMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/wificonfig", std::bind(&IotsaWifiMod::handler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/wificonfig", true, true);
  name = "wificonfig";
#endif
}

void IotsaWifiMod::configLoad() {
  IotsaConfigFileLoad cf("/config/wifi.cfg");
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
}

void IotsaWifiMod::configSave() {
  IotsaConfigFileSave cf("/config/wifi.cfg");
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  IFDEBUG IotsaSerial.println("Saved wifi.cfg");
}

void IotsaWifiMod::loop() {

#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (haveMDNS) MDNS.update();
#endif
  if (rebootAt && millis() > rebootAt) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
  // xxxjack
  if (!iotsaConfig.wifiPrivateNetworkMode) {
  	// Should be in normal mode, check that we have WiFi
  	static int disconnectedCount = 0;
  	if (WiFi.status() == WL_CONNECTED) {
  		if (disconnectedCount) {
  			IFDEBUG IotsaSerial.println("Wifi reconnected");
  		}
  		disconnectedCount = 0;
	} else {
		if (disconnectedCount == 0) {
			IFDEBUG IotsaSerial.println("Wifi connection lost");
		}
		disconnectedCount++;
		if (disconnectedCount > 60000) {
			IFDEBUG IotsaSerial.println("Wifi connection lost too long. Reboot.");
			ESP.restart();
		}
	}
  }
}
#endif // IOTSA_WITH_WIFI