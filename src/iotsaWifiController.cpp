#include "iotsaWifiController.h"
#include "iotsa.h"
#include "iotsaWifi.h"

#ifdef IOTSA_WITH_WIFI

//
// STUB -- slice 3a. This file exists so the interface compiles and links; the
// state machine / reconcile logic lands in slice 3b, at which point IotsaWifiMod
// starts calling begin()/tick() and the legacy _wifi* machinery is deleted.
// Until then nothing constructs or drives an IotsaWifiController.
//

void IotsaWifiController::begin() {
  // TODO 3b: seed desired state, first reconcile.
}

void IotsaWifiController::tick() {
  // TODO 3b: drain driver events -> _handleEvents(); run the deadlines; _reconcile().
}

void IotsaWifiController::setCredentials(const String &ssid, const String &psk) {
  bool changed = (_ssid != ssid) || (_psk != psk);
  _ssid = ssid;
  _psk = psk;
  if (changed) {
    // creds actually changed -> the fast-reconnect cache is stale
    _cache.valid = false;
  }
}

void IotsaWifiController::setRadioEnabled(bool on) { _radioEnabled = on; }
void IotsaWifiController::setConfigModeActive(bool active) { _configModeActive = active; }
void IotsaWifiController::credentialsChanged() { /* TODO 3b: force a prompt reconnect */ }

IotsaWifiApState IotsaWifiController::apState() const {
  if (!_apUp) return IotsaWifiApState::Off;
  return _apDisruptionSafe() ? IotsaWifiApState::On : IotsaWifiApState::InUse;
}

// ---- private, all TODO 3b ----

void IotsaWifiController::_reconcile() {}
bool IotsaWifiController::_wantApUp() const { return _configModeActive; }
void IotsaWifiController::_handleEvents(const IotsaWifiEvents &) {}
uint32_t IotsaWifiController::_retryDelayMillis() const { return 0; }
bool IotsaWifiController::_apDisruptionSafe() const { return true; }

#endif // IOTSA_WITH_WIFI
