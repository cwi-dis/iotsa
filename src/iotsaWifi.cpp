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

IotsaWifiMod::IotsaWifiMod(IotsaApplication &_app, IotsaAuthenticationProvider *_auth)
: IotsaModule(_app, _auth, true),
  ssid(""),
  ssidPassword("")
{
  // IotsaConfigMod is core infrastructure, not a WiFi sub-object -- it used to be a
  // member here purely so it got created, which meant a WiFi-less build lost
  // /api/config entirely (cwi-dis/iotsa#195). Create it via the shared singleton
  // instead, forwarding our auth provider; IotsaApplication::setup() also ensures
  // it, so it exists even when there's no WiFi module at all.
  IotsaConfigMod::ensure(_app, _auth);
}

void IotsaWifiMod::setup() {
  configLoad();  // also pushes credentials into the controller
  _installDriverEventHandlers();   // cwi-dis/iotsa#106
  _controller.setRadioEnabled(!iotsaConfig.wifiDisabledOnBoot);
  _controller.begin();
  // The controller does everything else from loop() -> tick(); it leaves the
  // radio untouched here so a wifiDisabledOnBoot device simply never turns it on.
}

void IotsaWifiMod::_publishControllerState() {
  const bool staConn = _controller.staConnected();
  const bool apAct = _controller.apActive();

  iotsaConfig.wifiStationConnected = staConn;
  iotsaConfig.wifiApActive = apAct;
  iotsaConfig.wifiEnabled = (_controller.staState() != IotsaWifiStaState::Off) || apAct;

  // Vestigial wifiMode -- kept written until slice 4 removes the enum and updates
  // getStatusColor() / privateWifi / networkIsUp() / inConfigurationOrFactoryMode().
  iotsa_wifi_mode m;
  if (staConn) m = IOTSA_WIFI_NORMAL;
  else if (_controller.staState() == IotsaWifiStaState::Connecting) m = IOTSA_WIFI_SEARCHING;
  else if (_controller.staState() == IotsaWifiStaState::Hunting) m = apAct ? IOTSA_WIFI_NOTFOUND : IOTSA_WIFI_SEARCHING;
  else m = apAct ? IOTSA_WIFI_FACTORY : IOTSA_WIFI_DISABLED;
  if (m != iotsaConfig.wifiMode) iotsaConfig.wifiMode = m;

  // mDNS follows the STA-connected / AP-active edges (each has its own IP).
  if ((staConn && !_lastStaConnected) || (apAct && !_lastApActive)) {
    _wifiStartMDNS();
  }
  if ((staConn != _lastStaConnected) || (apAct != _lastApActive)) {
    if (app.status) app.status->showStatus();
  }
  _lastStaConnected = staConn;
  _lastApActive = apAct;
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
#elif defined(IOTSA_HAS_COAPSERVER)
  const char *proto = "udp";
  const char *myproto = "coap";
  const int port = 5683;
#endif
  MDNS.addService(myproto, proto, port);
  MDNS.addService("iotsa", proto, port);
  MDNS.addServiceTxt("iotsa", proto, "P", myproto);
  MDNS.addServiceTxt("iotsa", proto, "V", IOTSA_FULL_VERSION);
  MDNS.addServiceTxt("iotsa", proto, "A", app.title.c_str());
  IotsaBaseModule *m = app.firstEarlyModule;
  while(m) {
    if (m->name != "" && m->hasApi()) MDNS.addServiceTxt("iotsa", proto, m->name.c_str(), "1");
    m = m->nextModule;
  }
  m = app.firstModule;
  while(m) {
    if (m->name != "" && m->hasApi()) MDNS.addServiceTxt("iotsa", proto, m->name.c_str(), "1");
    m = m->nextModule;
  }
 
  IFDEBUG IotsaSerial.println("MDNS responder started");
  iotsaConfig.mdnsEnabled = true;
  return true;
}

#ifdef IOTSA_WITH_WEB
void
IotsaWifiMod::webHandler() {
  bool wrongMode = false;
  if (needsAuthentication("config")) return;
  bool anyChanged = false;
  if( api.webService->server->hasArg("ssid")) {
    if (iotsaConfig.inConfigurationOrFactoryMode()) {
      ssid = api.webService->server->arg("ssid");
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if( api.webService->server->hasArg("ssidPassword")) {
    if (iotsaConfig.inConfigurationOrFactoryMode()) {
      ssidPassword = api.webService->server->arg("ssidPassword");
      anyChanged = true;
    } else {
      wrongMode = true;
    }
  }
  if (api.webService->server->hasArg("wifiPowerReduction")) {
    int val = api.webService->server->arg("wifiPowerReduction").toInt();
    if ((bool) val != wifiPowerReduction) {
      if (iotsaConfig.inConfigurationOrFactoryMode()) {
        wifiPowerReduction = (bool)val;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
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
  message += "'><br>Password: <input type='password' name='ssidPassword'>";
  message += "<br>WiFi Power Reduction: <input type='checkbox' name='wifiPowerReduction'";
  if (wifiPowerReduction) message += " checked";
  message += "><br> (work around issue on some esp32c3 boards)<br>";
  message += "<br><input type='submit'>";
  message += "</form>";
  if (iotsaConfig.inConfigurationOrFactoryMode()) {
#ifdef ESP32
    uint8_t baseMac[6];
    char baseMacStr[32];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
      snprintf(baseMacStr, sizeof(baseMacStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                    baseMac[0], baseMac[1], baseMac[2],
                    baseMac[3], baseMac[4], baseMac[5]);
      message += "<p>WiFi MAC address: <code>";
      message += baseMacStr;
      message += "</code></p>";
    } else {
      message += "<p>Cannot determine MAC address.</p>";
    }
#else
    message += "<p>WiFi MAC address: <code>";
    message += WiFi.macAddress();
    message += "</code></p>";
#endif
  }

  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
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

bool IotsaWifiMod::getHandler(const char *path, JsonObject& reply) {
  reply["ssid"] = ssid;
  reply["has_ssidPassword"] = ssidPassword.length() > 0;
  reply["wifiPowerReduction"] = wifiPowerReduction;
  return true;
}

bool IotsaWifiMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  if (!iotsaConfig.inConfigurationOrFactoryMode()) {
    IFDEBUG IotsaSerial.println("wificonfig: Not in config mode");
    return false;
  }
  JsonObject reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "ssid", ssid)) {
    anyChanged = true;
  }
  if (getFromRequest<const char *>(reqObj, "ssidPassword", ssidPassword)) {
    anyChanged = true;
  }
  if (getFromRequest<bool>(reqObj, "wifiPowerReduction", wifiPowerReduction)) {
    anyChanged = true;
  }
  if (anyChanged) configSave();
  if (reqObj["reboot"]) {
    iotsaConfig.requestReboot(2000);
  }
  checkUnhandled(reqObj);
  return anyChanged;
}

void IotsaWifiMod::lateSetup() {
  api.setup("wificonfig", true, true);
  name = "wificonfig";
}

void IotsaWifiMod::configLoad() {
  IotsaConfigFileLoad cf("/config/wifi.cfg");
  cf.get("ssid", ssid, "");
  cf.get("ssidPassword", ssidPassword, "");
  cf.get("wifiPowerReduction", wifiPowerReduction,
#ifdef ESP32C3
    true
#else
    false
#endif
  );
  _controller.setCredentials(ssid, ssidPassword);
}

void IotsaWifiMod::configSave() {
  IotsaConfigFileSave cf("/config/wifi.cfg");
  cf.put("ssid", ssid);
  cf.put("ssidPassword", ssidPassword);
  cf.put("wifiPowerReduction", wifiPowerReduction);
  IFDEBUG IotsaSerial.println("Saved wifi.cfg");
  // Persist only. The old factory->beginConfigurationMode() side effect and the
  // wantWifiModeSwitchAtMillis poke are gone (cwi-dis/iotsa#106): the request
  // handler tells the controller explicitly via credentialsChanged().
  _controller.setCredentials(ssid, ssidPassword);
  _controller.credentialsChanged();
}

void IotsaWifiMod::loop() {
  _controller.setConfigModeActive(iotsaConfig.inConfigurationMode());
  _controller.tick();
  _publishControllerState();
#ifndef ESP32
  // mDNS happens asynchronously on ESP32
  if (iotsaConfig.mdnsEnabled) MDNS.update();
#endif
}

// ===========================================================================
// Driver surface (cwi-dis/iotsa#106). Policy-free mechanism, called by
// IotsaWifiController.
// ===========================================================================

IotsaWifiStaFailReason IotsaWifiMod::_reduceStaFailReason(int reason) {
  // The numeric values line up between the ESP8266 (WIFI_DISCONNECT_REASON_*) and
  // ESP32 (WIFI_REASON_*) enums -- both extend the 802.11 spec reason codes.
  switch (reason) {
    case 201: // NO_AP_FOUND
      return IotsaWifiStaFailReason::NoApFound;
    case 202: // AUTH_FAIL
    case 15:  // 4WAY_HANDSHAKE_TIMEOUT
    case 204: // HANDSHAKE_TIMEOUT -- a wrong password often surfaces this way
      return IotsaWifiStaFailReason::AuthFail;
    default:
      return IotsaWifiStaFailReason::Other;
  }
}

void IotsaWifiMod::_installDriverEventHandlers() {
  if (_driverHandlersInstalled) return;
  _driverHandlersInstalled = true;
#ifdef ESP32
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#ifdef ARDUINO_EVENT_WIFI_STA_GOT_IP6
      case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
#endif
        _evLastChannel = WiFi.channel();
        { const uint8_t* b = WiFi.BSSID(); if (b) memcpy(_evLastBssid, b, 6); }
        _driverHaveIp = true;
        _evStaGotIp = true;
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (_driverHaveIp) { _evStaLost = true; _driverHaveIp = false; }
        else { _evStaFailReason = (uint8_t)_reduceStaFailReason(info.wifi_sta_disconnected.reason); _evStaFailed = true; }
        break;
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        _evApClientCountChanged = true;
        break;
      default:
        break;
    }
  });
#else
  _evH_gotIp = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP&) {
    _evLastChannel = WiFi.channel();
    const uint8_t* b = WiFi.BSSID();
    if (b) memcpy(_evLastBssid, b, 6);
    _driverHaveIp = true;
    _evStaGotIp = true;
  });
  _evH_disconnected = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& e) {
    if (_driverHaveIp) { _evStaLost = true; _driverHaveIp = false; }
    else { _evStaFailReason = (uint8_t)_reduceStaFailReason((int)e.reason); _evStaFailed = true; }
  });
  _evH_apConnect = WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected&) {
    _evApClientCountChanged = true;
  });
  _evH_apDisconnect = WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected&) {
    _evApClientCountChanged = true;
  });
#endif
}

IotsaWifiEvents IotsaWifiMod::drainEvents() {
  IotsaWifiEvents e;
  // Read-and-clear. Single-word volatile ops; the foreign-context callbacks only
  // ever set these, so a lost race just defers an event one tick -- acceptable
  // here, hardening tracked in cwi-dis/iotsa#236.
  if (_evStaGotIp) {
    e.staGotIp = true;
    e.lastChannel = _evLastChannel;
    memcpy(e.lastBssid, _evLastBssid, 6);
    _evStaGotIp = false;
  }
  if (_evStaFailed) {
    e.staFailed = true;
    e.staFailReason = (IotsaWifiStaFailReason)_evStaFailReason;
    _evStaFailed = false;
  }
  if (_evStaLost) { e.staLost = true; _evStaLost = false; }
  if (_evApClientCountChanged) { e.apClientCountChanged = true; _evApClientCountChanged = false; }
  return e;
}

IotsaWifiActualState IotsaWifiMod::readActualState() const {
  IotsaWifiActualState st;
  int mode = (int)WiFi.getMode();
  st.staEnabled = (mode & (int)WIFI_STA) != 0;
  st.apEnabled = (mode & (int)WIFI_AP) != 0;
  wl_status_t link = WiFi.status();
  st.staLinkStatus = (int)link;
  st.staConnected = (link == WL_CONNECTED);
  if (st.staConnected) st.staChannel = WiFi.channel();
#ifdef ESP32
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
    st.staConfiguredSsid = String((const char *)conf.sta.ssid);
    st.staConfiguredPsk = String((const char *)conf.sta.password);
  }
#else
  st.staConfiguredSsid = WiFi.SSID();
  st.staConfiguredPsk = WiFi.psk();
#endif
  if (st.apEnabled) {
    st.apSsid = WiFi.softAPSSID();
    st.apClientCount = WiFi.softAPgetStationNum();
  }
  return st;
}

bool IotsaWifiMod::startStation(const String& targetSsid, const String& targetPsk, uint8_t channel, const uint8_t* bssid) {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_STA);
  if (!WiFi.mode(newMode)) return false;
  wl_status_t sts;
  if (channel != 0 && bssid != nullptr) {
    sts = WiFi.begin(targetSsid.c_str(), targetPsk.c_str(), channel, bssid);
  } else {
    sts = WiFi.begin(targetSsid.c_str(), targetPsk.c_str());
  }
#ifdef ESP32
  if (wifiPowerReduction) WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  WiFi.setAutoReconnect(true);
  return sts != WL_CONNECT_FAILED;
}

void IotsaWifiMod::stopStation() {
  WiFi.disconnect(false); // keep stored credentials
  WiFi.mode((WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_STA));
  _driverHaveIp = false;
}

bool IotsaWifiMod::startAP(const String& apName) {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_AP);
  if (!WiFi.mode(newMode)) return false;
  return WiFi.softAP(apName.c_str());
}

void IotsaWifiMod::stopAP() {
  WiFi.softAPdisconnect(false);
  WiFi.mode((WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_AP));
}

void IotsaWifiMod::reinitStack() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(10);
  WiFi.mode(WIFI_STA);
  _driverHaveIp = false;
}
#endif // IOTSA_WITH_WIFI