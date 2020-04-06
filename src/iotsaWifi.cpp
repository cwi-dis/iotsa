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
#ifdef ESP32
#include <esp_wifi.h>
#endif

#ifdef IOTSA_WITH_WIFI

static int privateNetworkModeReason;

#ifdef ESP32
void IotsaWifiMod::_wifiCallback(system_event_id_t event, system_event_info_t info) {
  IFDEBUG IotsaSerial.printf("WiFi Event %d:\n", event);
}
#else
static void _wifiStaticCallback(WiFiEvent_t event) {
  IotsaSerial.printf("WiFi Event %d:\n", (int)event);
}
#endif

IotsaWifiMod::IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth)
: IotsaWifiModBaseMod(_app, _auth, true),
  configMod(_app, _auth),
  ssid(""),
  ssidPassword(""),
  wantWifiModeSwitch(false),
  searchTimeoutMillis(0)
{
}

void IotsaWifiMod::setup() {
#ifdef ESP32
  WiFi.onEvent(std::bind(&IotsaWifiMod::_wifiCallback, this, std::placeholders::_1, std::placeholders::_2));
#else
  WiFi.onEvent(_wifiStaticCallback);
#endif
  IFDEBUG IotsaSerial.printf("xxxjack wifi: boot: mode=%d, status=%d\n", WiFi.getMode(), WiFi.status());
  if (iotsaConfig.disableWifiOnBoot) {
    IFDEBUG IotsaSerial.println("WiFi disabled");
    WiFi.mode(WIFI_OFF);
    iotsaConfig.wifiMode = IOTSA_WIFI_DISABLED;
    if (app.status) app.status->showStatus();
    wantWifiModeSwitch = false;
    return;
  }
  configLoad();
  iotsaConfig.wifiEnabled = true;
  _wifiGotoMode();
}

void IotsaWifiMod::_wifiGotoMode() {
  iotsa_wifi_mode newMode = IOTSA_WIFI_DISABLED;
  if (ssid.length()) {
    newMode = IOTSA_WIFI_NORMAL;
  } else {
    newMode = IOTSA_WIFI_FACTORY;
  }
  if (newMode == IOTSA_WIFI_DISABLED) {
    _wifiStopStation();
    _wifiStopAP(IOTSA_WIFI_DISABLED);
  } else if (newMode == IOTSA_WIFI_FACTORY) {
    _wifiStopStation();
    IFDEBUG IotsaSerial.printf("xxxjack wifi: startAP: mode=%d, status=%d\n", WiFi.getMode(), WiFi.status());
    _wifiStartAP(IOTSA_WIFI_FACTORY);
  } else {
    _wifiStartStation();
    IFDEBUG IotsaSerial.printf("xxxjack wifi: startStation: mode=%d, status=%d\n", WiFi.getMode(), WiFi.status());
  }
}

bool IotsaWifiMod::_wifiStartStation() {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_STA);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(WIFI_STA (%d)) failed", (int)newMode);
    return false;
  }
  wl_status_t sts = WiFi.begin(ssid.c_str(), ssidPassword.c_str());
  if (sts == WL_CONNECT_FAILED) {
    IotsaSerial.println("WiFi.begin(...) failed");
    return false;
  }
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  IFDEBUG IotsaSerial.println("");
  iotsaConfig.wifiMode = IOTSA_WIFI_SEARCHING;
  searchTimeoutMillis = millis() + IOTSA_WIFI_TIMEOUT*1000;
  if (app.status) app.status->showStatus();
  return true;
}

void IotsaWifiMod::_wifiStopStation() {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_STA);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(not WIFI_STA (%d)) failed", (int)newMode);
    return;
  }
  // xxxjack set iotsaConfig.wifiMode?
}

bool IotsaWifiMod::_wifiWaitStation() {
  // Wait for connection
  int count = IOTSA_WIFI_TIMEOUT;
  while (WiFi.status() != WL_CONNECTED && count > 0) {
    delay(1000);
    IFDEBUG IotsaSerial.print(".");
    count--;
  }
  return count > 0;
}

void IotsaWifiMod::_wifiStartStationSucceeded() {
  iotsaConfig.wifiMode = IOTSA_WIFI_NORMAL;
  if (app.status) app.status->showStatus();
  IFDEBUG IotsaSerial.println("");
  IFDEBUG IotsaSerial.print("Connected to ");
  IFDEBUG IotsaSerial.println(ssid);
  IFDEBUG IotsaSerial.print("IP address: ");
  IFDEBUG IotsaSerial.println(WiFi.localIP());
  
  WiFi.setAutoReconnect(true);
  _wifiStartMDNS();
}

void IotsaWifiMod::_wifiStartStationFailed() {
  iotsaConfig.wifiMode = IOTSA_WIFI_NOTFOUND;
  if (app.status) app.status->showStatus();
  privateNetworkModeReason = WiFi.status();
  iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
  IFDEBUG IotsaSerial.print("Cannot join ");
  IFDEBUG IotsaSerial.print(ssid);
  IFDEBUG IotsaSerial.print(", status=");
  IFDEBUG IotsaSerial.println(privateNetworkModeReason);
  IFDEBUG IotsaSerial.print(", retry at ");
  IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
}

bool IotsaWifiMod::_wifiStartAP(iotsa_wifi_mode mode) {
  String networkName = "config-" + iotsaConfig.hostName;
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_AP);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(WIFI_AP (%d)) failed", (int)newMode);
    return false;
  }

  if (!WiFi.softAP(networkName.c_str())) {
    IotsaSerial.println("WiFi.SoftAP(...) failed");
    return false;
  }
  IFDEBUG IotsaSerial.print("\nCreating softAP for network ");
  IFDEBUG IotsaSerial.println(networkName);
  IFDEBUG IotsaSerial.print("IP address: ");
  IFDEBUG IotsaSerial.println(WiFi.softAPIP());
  iotsaConfig.wifiMode = mode;
  if (app.status) app.status->showStatus();
  _wifiStartMDNS();
  return true;
}

void IotsaWifiMod::_wifiStopAP(iotsa_wifi_mode mode) {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_AP);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(WIFI_AP (%d)) failed", (int)newMode);
    return;
  }
  if (app.status) app.status->showStatus();
  iotsaConfig.wifiMode = mode;
}

void IotsaWifiMod::_wifiOff() {
  WiFi.mode(WIFI_OFF);
  iotsaConfig.wifiMode = IOTSA_WIFI_DISABLED;
}

bool IotsaWifiMod::_wifiStartMDNS() {
  MDNS.end();
  if (!MDNS.begin(iotsaConfig.hostName.c_str())) {
    IotsaSerial.println("MDNS.begin(...) failed");
    return false;
  }
#ifdef IOTSA_WITH_HTTPS
  MDNS.addService("https", "tcp", 443);
  // MDNS.addService("iotsa._sub._https", "tcp", 443);
  MDNS.addService("iotsa", "tcp", 443);
#endif
#ifdef IOTSA_WITH_HTTP
  MDNS.addService("http", "tcp", 80);
  // MDNS.addService("iotsa._sub._http", "tcp", 80);
#ifndef IOTSA_WITH_HTTPS
  MDNS.addService("iotsa", "tcp", 80);
#endif
#endif
#ifdef IOTSA_WITH_COAP
  MDNS.addService("coap", "udp", 5683);
  // MDNS.addService("iotsa._sub._coap", "udp", 5683);
  MDNS.addService("iotsa", "udp", 5683);
#endif
  IFDEBUG IotsaSerial.println("MDNS responder started");
  iotsaConfig.mdnsEnabled = true;
  return true;
}

#ifdef IOTSA_WITH_WEB
void
IotsaWifiMod::handler() {
  bool wrongMode = false;
  if (needsAuthentication("config")) return;
  bool anyChanged = false;
  if( server->hasArg("ssid")) {
    if (iotsaConfig.inConfigurationOrFactoryMode()) {
      ssid = server->arg("ssid");
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if( server->hasArg("ssidPassword")) {
    if (iotsaConfig.inConfigurationOrFactoryMode()) {
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
  } else if(!iotsaConfig.inConfigurationOrFactoryMode()) {
    message += "<p><i>(Note: you must be in configuration mode to change WiFi settings)</i></p>";
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
    iotsaConfig.requestReboot(2000);
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
  if (!iotsaConfig.mdnsEnabled) {
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
  reply["has_ssidPassword"] = ssidPassword.length() > 0;
  return true;
}

bool IotsaWifiMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  if (!iotsaConfig.inConfigurationOrFactoryMode()) {
    IFDEBUG IotsaSerial.println("Not in config mode");
    return false;
  }
  JsonObject reqObj = request.as<JsonObject>();
  if (reqObj.containsKey("ssid")) {
    ssid = reqObj["ssid"].as<String>();
    anyChanged = true;
  }
  if (reqObj.containsKey("ssidPassword")) {
    ssidPassword = reqObj["ssidPassword"].as<String>();
    anyChanged = true;
  }
  if (anyChanged) configSave();
  if (reqObj["reboot"]) {
    iotsaConfig.requestReboot(2000);
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
  if (wantWifiModeSwitch) {
    //
    // Either setup() or saveConfig() asked to change the WiFi mode. Do so.
    //
    wantWifiModeSwitch = false;
    _wifiGotoMode();
  }
  //
  // Depending on the current mode, check whether we need to change
  // it due to the current WiFi status
  //
  wl_status_t curStatus = WiFi.status();
  switch(iotsaConfig.wifiMode) {
  case IOTSA_WIFI_DISABLED:
    if (curStatus != WL_IDLE_STATUS && curStatus != WL_NO_SHIELD) {
      IFDEBUG IotsaSerial.printf("WiFi disabled, but status=%d\n", (int)curStatus);
    }
    break;
  case IOTSA_WIFI_FACTORY:
    break;
  case IOTSA_WIFI_NORMAL:
    if (curStatus != WL_CONNECTED) {
      //
      // Lost connection. 
      IFDEBUG IotsaSerial.println("WiFi connection lost");
      iotsaConfig.wifiMode = IOTSA_WIFI_SEARCHING;
      searchTimeoutMillis = millis() + IOTSA_WIFI_TIMEOUT*1000;
    }
    break;
  case IOTSA_WIFI_NOTFOUND:
  case IOTSA_WIFI_SEARCHING:
    if (curStatus == WL_CONNECTED) {
      // Search succeeded, we are connected.
      searchTimeoutMillis = 0;
      _wifiStartStationSucceeded();
      // The AP may be enabled or not, disabling it anyway.
      _wifiStopAP(IOTSA_WIFI_NORMAL);
    } else if (millis() > searchTimeoutMillis) {
      // Search failed. Enable AP. Continue searching,
      // but we don't need to enable the AP again.
      searchTimeoutMillis = 0;
      _wifiStartStationFailed();
      _wifiStartAP(IOTSA_WIFI_NOTFOUND);
    }
    break;
  }
#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (iotsaConfig.mdnsEnabled) MDNS.update();
#endif
  if (!iotsaConfig.wifiEnabled) return;
  // xxxjack
  if (iotsaConfig.wifiMode == IOTSA_WIFI_NORMAL) {
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