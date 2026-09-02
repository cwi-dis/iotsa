#include "iotsaBuildOptions.h"   // IOTSA_WITH_WIFI
#include "iotsaWifiDriver.h"
#include <string.h>
#ifdef ESP32
#include <esp_wifi.h>
#endif

#ifdef IOTSA_WITH_WIFI

IotsaWifiStaFailReason IotsaWifiDriver::_reduceStaFailReason(int reason) {
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

void IotsaWifiDriver::begin() {
  if (_handlersInstalled) return;
  _handlersInstalled = true;
  WiFi.setAutoReconnect(_autoReconnect);   // on by default; the controller may flip it
#ifdef ESP32
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        _l2Associated = true;     // associated + authed; DHCP still to come
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
#ifdef ARDUINO_EVENT_WIFI_STA_GOT_IP6
      case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
#endif
        _evLastChannel = WiFi.channel();
        { const uint8_t *b = WiFi.BSSID(); if (b) memcpy(_evLastBssid, b, 6); }
        _haveIp = true;
        _evStaGotIp = true;
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        _l2Associated = false;
        if (_haveIp) { _evStaLost = true; _haveIp = false; }
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
  _evH_gotIp = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP &) {
    _evLastChannel = WiFi.channel();
    const uint8_t *b = WiFi.BSSID();
    if (b) memcpy(_evLastBssid, b, 6);
    _haveIp = true;
    _evStaGotIp = true;
  });
  _evH_connected = WiFi.onStationModeConnected([this](const WiFiEventStationModeConnected &) {
    _l2Associated = true;         // associated + authed; DHCP still to come
  });
  _evH_disconnected = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected &e) {
    _l2Associated = false;
    if (_haveIp) { _evStaLost = true; _haveIp = false; }
    else { _evStaFailReason = (uint8_t)_reduceStaFailReason((int)e.reason); _evStaFailed = true; }
  });
  _evH_apConnect = WiFi.onSoftAPModeStationConnected([this](const WiFiEventSoftAPModeStationConnected &) {
    _evApClientCountChanged = true;
  });
  _evH_apDisconnect = WiFi.onSoftAPModeStationDisconnected([this](const WiFiEventSoftAPModeStationDisconnected &) {
    _evApClientCountChanged = true;
  });
#endif
}

void IotsaWifiDriver::setAutoReconnect(bool on) {
  _autoReconnect = on;
  WiFi.setAutoReconnect(on);
  if (!on) WiFi.disconnect(false);   // halt the SDK's in-flight retry loop, keep stored creds
}

IotsaWifiEvents IotsaWifiDriver::drainEvents() {
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

IotsaWifiActualState IotsaWifiDriver::readActualState() const {
  IotsaWifiActualState st;
  int mode = (int)WiFi.getMode();
  st.staEnabled = (mode & (int)WIFI_STA) != 0;
  st.apEnabled = (mode & (int)WIFI_AP) != 0;
  wl_status_t link = WiFi.status();
  st.staLinkStatus = (int)link;
  st.staConnected = (link == WL_CONNECTED);
  st.staAssociated = _l2Associated;
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

bool IotsaWifiDriver::startStation(const String &ssid, const String &psk, uint8_t channel, const uint8_t *bssid) {
  _l2Associated = false;
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_STA);
  if (!WiFi.mode(newMode)) return false;
  wl_status_t sts;
  if (channel != 0 && bssid != nullptr) {
    sts = WiFi.begin(ssid.c_str(), psk.c_str(), channel, bssid);
  } else {
    sts = WiFi.begin(ssid.c_str(), psk.c_str());
  }
#ifdef ESP32
  if (_txPowerReduction) WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  WiFi.setAutoReconnect(_autoReconnect);   // some SDK versions reset this in begin()
  return sts != WL_CONNECT_FAILED;
}

void IotsaWifiDriver::stopStation() {
  WiFi.disconnect(false); // keep stored credentials
  WiFi.mode((WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_STA));
  _haveIp = false;
  _l2Associated = false;
}

bool IotsaWifiDriver::startAP(const String &apName) {
  WiFiMode_t newMode = (WiFiMode_t)((int)WiFi.getMode() | (int)WIFI_AP);
  if (!WiFi.mode(newMode)) return false;
  return WiFi.softAP(apName.c_str());
}

void IotsaWifiDriver::stopAP() {
  WiFi.softAPdisconnect(false);
  WiFi.mode((WiFiMode_t)((int)WiFi.getMode() & ~(int)WIFI_AP));
}

void IotsaWifiDriver::reinitStack() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(10);
  WiFi.mode(WIFI_STA);
  _haveIp = false;
  _l2Associated = false;
}

#endif // IOTSA_WITH_WIFI
