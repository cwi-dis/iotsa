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
#include "iotsaWifi.h"

#define WIFI_TIMEOUT 30
int privateNetworkModeReason;

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
    int count = WIFI_TIMEOUT;
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
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("iotsa", "tcp", 80);
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

void
IotsaWifiMod::handler() {
  bool anyChanged = false;
  for (uint8_t i=0; i<server.args(); i++){
    if( server.argName(i) == "ssid") {
      ssid = server.arg(i);
      anyChanged = true;
    }
    if( server.argName(i) == "ssidPassword") {
      ssidPassword = server.arg(i);
      anyChanged = true;
    }
    if (anyChanged) {
    	configSave();
    }
  }
  String message = "<html><head><title>WiFi configuration</title></head><body><h1>WiFi configuration</h1>";
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
  message += "<p>Hostname: ";
  message += htmlEncode(iotsaConfig.hostName);
  message += ", see <a href='/config'>/config</a> to change.</p>";
  message += "<form method='get'>Network: <input name='ssid' value='";
  message += htmlEncode(ssid);
  message += "'><br>Password: <input type='password' name='ssidPassword' value='";
  message += htmlEncode(ssidPassword);
  message += "'><br><input type='submit'></form>";
  message += "</body></html>";
  server.send(200, "text/html", message);
  if (anyChanged) {
    if (app.status) app.status->showStatus();
    IFDEBUG IotsaSerial.print("Restart in 2 seconds");
    delay(2000);
    ESP.restart();
  }
}

bool IotsaWifiMod::getHandler(const char *path, JsonObject& reply) {
  reply["ssid"] = ssid;
  reply["ssidPassword"] = ssidPassword;
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
    ssid = reqObj.get<String>("ssidPassword");
    anyChanged = true;
  }
  if (anyChanged) configSave();
  if (reqObj.get<bool>("reboot")) {
    delay(2000);
    ESP.restart();
  }
  return anyChanged;
}

void IotsaWifiMod::serverSetup() {
  server.on("/wificonfig", std::bind(&IotsaWifiMod::handler, this));
  api.setup("/api/wificonfig", true, true);
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
