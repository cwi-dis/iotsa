#include <Esp.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <esp_log.h>
#else
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaWifi.h"
#ifdef ESP32
#include <esp_wifi.h>
#endif

#ifdef IOTSA_WITH_WIFI

static int privateNetworkModeReason;

IotsaWifiMod::IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth)
: IotsaWifiModBaseMod(_app, _auth, true),
  configMod(_app, _auth),
  ssid(""),
  ssidPassword(""),
  searchTimeoutMillis(0)
{
}

void IotsaWifiMod::setup() {
  if (iotsaConfig.wifiDisabledOnBoot) {
    IFDEBUG IotsaSerial.println("WiFi disabled by iotsaBattery");
    WiFi.mode(WIFI_OFF);
    iotsaConfig.wifiMode = iotsa_wifi_mode::IOTSA_WIFI_DISABLED;
    if (app.status) app.status->showStatus();
    iotsaConfig.wantWifiModeSwitchAtMillis = 0;
    return;
  } else {
    // Otherwise we presume Normal mode, which will revert to factory if we have no SSID.
    iotsaConfig.wifiMode = iotsa_wifi_mode::IOTSA_WIFI_NORMAL;
  }
  configLoad();
  _wifiGotoMode();
}

void IotsaWifiMod::_wifiGotoMode() {
  if (iotsaConfig.wifiMode != iotsa_wifi_mode::IOTSA_WIFI_DISABLED) {
    configLoad();
  }
  if (iotsaConfig.wifiMode == iotsa_wifi_mode::IOTSA_WIFI_NORMAL) {
    if (ssid.length() == 0) {
      // If no ssid is configured we revert to fatory mode.
      iotsaConfig.wifiMode = iotsa_wifi_mode::IOTSA_WIFI_FACTORY;
    }
  }
  if (iotsaConfig.wifiMode == IOTSA_WIFI_DISABLED) {
    _wifiStopStation();
    _wifiStopAP(IOTSA_WIFI_DISABLED);
    iotsaConfig.wifiEnabled = false;
    return;
  }
  iotsaConfig.wifiEnabled = true;
  if (iotsaConfig.wifiMode == IOTSA_WIFI_FACTORY) {
    _wifiStopStation();
    _wifiStartAP(IOTSA_WIFI_FACTORY);
  } else {
    if (iotsaConfig.inConfigurationMode()) {
      // In configuration mode we want both AP and STA
      if ((int)WiFi.getMode() & (int)WIFI_AP) {
        ; // Already in AP mode. Leave it so.
      } else {
        _wifiStartAP(IOTSA_WIFI_SEARCHING);
      }
    } else {
      _wifiStopAP(IOTSA_WIFI_SEARCHING);
    }
    _wifiStartStation();
  }
}

bool IotsaWifiMod::_wifiStartStation() {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_STA);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(WIFI_STA (%d)) failed", (int)newMode);
    return false;
  }
  IFDEBUG IotsaSerial.print("Connecting to ");
  IFDEBUG IotsaSerial.println(ssid);
  wl_status_t sts = WiFi.begin(ssid.c_str(), ssidPassword.c_str());
  if (sts == WL_CONNECT_FAILED) {
    IotsaSerial.println("WiFi.begin(...) failed");
    return false;
  }
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
  IFDEBUG IotsaSerial.println("Disconnecting from WiFi network");
}

void IotsaWifiMod::_wifiStartStationSucceeded() {
  iotsaConfig.wifiMode = IOTSA_WIFI_NORMAL;
  if (app.status) app.status->showStatus();
  IFDEBUG IotsaSerial.print("");
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
  IFDEBUG IotsaSerial.print("Cannot connect to ");
  IFDEBUG IotsaSerial.print(ssid);
  IFDEBUG IotsaSerial.print(", status=");
  IFDEBUG IotsaSerial.println(privateNetworkModeReason);
  // Stop searching, so we can enable AP.
  _wifiStopStation(); 
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
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_AP);
  if (!WiFi.mode(newMode)) {
    IotsaSerial.printf("WiFi.mode(WIFI_AP (%d)) failed", (int)newMode);
    return;
  }
  iotsaConfig.wifiMode = mode;
  if (app.status) app.status->showStatus();
  IFDEBUG IotsaSerial.println("SoftAP turned off");
}

void IotsaWifiMod::_wifiOff() {
  WiFi.mode(WIFI_OFF);
  iotsaConfig.wifiMode = IOTSA_WIFI_DISABLED;
  if (app.status) app.status->showStatus();
  IFDEBUG IotsaSerial.println("WiFi turned off");
}

bool IotsaWifiMod::_wifiStartMDNS() {
  MDNS.end();
  if (!MDNS.begin(iotsaConfig.hostName.c_str())) {
    IotsaSerial.println("MDNS.begin(...) failed");
    return false;
  }
#if defined(IOTSA_WITH_HTTPS)
  const char *proto = "tcp";
  const char *myproto = "https";
  const int port = 443;
#elif defined(IOTSA_WITH_HTTP)
  const char *proto = "tcp";
  const char *myproto = "http";
  const int port = 80;
#elif defined(IOTSA_WITH_COAP)
  const char *proto = "udp";
  const char *myproto = "coap";
  const int port = 5683;
#endif
  MDNS.addService(myproto, proto, port);
  MDNS.addService("iotsa", proto, port);
  MDNS.addServiceTxt("iotsa", proto, "P", myproto);
  MDNS.addServiceTxt("iotsa", proto, "V", IOTSA_FULL_VERSION);
  MDNS.addServiceTxt("iotsa", proto, "A", app.title.c_str());
  IotsaBaseMod *m = app.firstEarlyModule;
  while(m) {
    MDNS.addServiceTxt("iotsa", proto, m->name.c_str(), "1");
    m = m->nextModule;
  }
  m = app.firstModule;
  while(m) {
    MDNS.addServiceTxt("iotsa", proto, m->name.c_str(), "1");
    m = m->nextModule;
  }
 
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
#if 0
  if (anyChanged) {
    message += "<p>Settings saved to EEPROM. <em>Rebooting device to activate new settings.</em></p>";
  }
#endif
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
#if 0
  // Reboot is no longer needed, config change handled by changing wifi on the fly
  if (anyChanged) {
    if (app.status) app.status->showStatus();
    iotsaConfig.requestReboot(2000);
  }
#endif
}

String IotsaWifiMod::info() {
  IPAddress x;
  String message = "<p>WiFi mode: " + String((int)WiFi.getMode()) + ", iotsaWifiMode: " + String((int)iotsaConfig.wifiMode) + ", WiFi status: " + String((int)WiFi.status()) + ".</p>";
  message += "<p>IP address is ";
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
  if (reqObj["ssid"].is<const char *>()) {
    ssid = reqObj["ssid"].as<String>();
    anyChanged = true;
  }
  if (reqObj["ssidPassword"].is<const char *>()) {
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
  // If we were in factory mode enable config mode (so we keep the AP)
  if (iotsaConfig.wifiMode == IOTSA_WIFI_FACTORY) {
    iotsaConfig.beginConfigurationMode();
  }
  // And we always want to connect to the new SSID as STA.
  iotsaConfig.wantWifiModeSwitchAtMillis = millis();
}

void IotsaWifiMod::loop() {
  if (iotsaConfig.wantWifiModeSwitchAtMillis > 0 && iotsaConfig.wantWifiModeSwitchAtMillis < millis()) {
    //
    // Either setup() or saveConfig() or configuration mode change asked to change the WiFi mode. Do so.
    //
    iotsaConfig.wantWifiModeSwitchAtMillis = 0;
    _wifiGotoMode();
  }
  //
  // Depending on the current mode, check whether we need to change
  // it due to the current WiFi status
  //
  wl_status_t curStatus = WiFi.status();
#if 0
  {
    static uint32_t last;
    if (millis() > last + 5000) {
      IotsaSerial.printf("WiFi.status() == %d\n", curStatus);
      last = millis();
    }
  }
#endif
  switch(iotsaConfig.wifiMode) {
  case IOTSA_WIFI_DISABLED:
    if (iotsaConfig.wantWifiModeSwitchAtMillis== 0 && curStatus != WL_IDLE_STATUS && curStatus != WL_NO_SHIELD) {
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
    // We are in AP mode because the network we really want couldn't be connected to.
    // We stay in this mode until we time out, then we try connecting again.
    if (searchTimeoutMillis != 0 && millis() > searchTimeoutMillis) {
      IotsaSerial.println("Wifi retry connecting");
      _wifiStartStation();
    }
    break;
  case IOTSA_WIFI_SEARCHING:
    if (curStatus == WL_CONNECTED) {
      // Search succeeded, we are connected.
      searchTimeoutMillis = 0;
      _wifiStartStationSucceeded();
      // The AP may be enabled or not, disabling it anyway, unless we are in configuration mode
      if (!iotsaConfig.inConfigurationMode()) {
        _wifiStopAP(IOTSA_WIFI_NORMAL);
      }
    } else if (searchTimeoutMillis != 0 && millis() > searchTimeoutMillis) {
      // Search failed. Enable AP.
      // Unfortunately we cannot continue searching (because this requires the radio to hop channels,
      // which would kill the AP).
      // We stay in AP mode for ten times search duration (300 seconds default)
      searchTimeoutMillis = millis() + 10*IOTSA_WIFI_TIMEOUT*1000;
      if (curStatus == WL_IDLE_STATUS || curStatus == WL_NO_SHIELD) {
        // Unsure why this happens, maybe only when switching from one SSID to another?
        // We think we are searching but the WiFi thinks nothing is happening.
        // Reboot.
        IotsaSerial.println("WiFi unexpectedly gone idle. Rebooting...");
        iotsaConfig.requestReboot(2000);
      }
      _wifiStartStationFailed();
      _wifiStartAP(IOTSA_WIFI_NOTFOUND);
    }
    break;
  }
#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (iotsaConfig.mdnsEnabled) MDNS.update();
#endif
}
#endif // IOTSA_WITH_WIFI