#include "iotsaWifiController.h"
#include "iotsa.h"   // for iotsaConfig (hostName)

#ifdef IOTSA_WITH_WIFI

//
// IotsaWifiController -- the WiFi policy layer. See docs/wifi-controller-design.md.
// IotsaWifiMod owns one, feeds it desired state (setCredentials / setRadioEnabled /
// setConfigModeActive) each tick, and reads back staState() / apState() to publish
// into iotsaConfig. The controller drives the radio only through the driver.
//

// Tunables (docs "Open questions" -- adjusted against real hardware, cwi-dis/iotsa#106).
static const uint32_t TAKEOVER_MS      = 10UL * 1000UL;   // STA down this long w/ SDK auto-reconnect -> take over
static const uint32_t HUNT_WINDOW_MS   = 10UL * 1000UL;   // one STA hunt window (also the mid-DHCP grace extension)
static const uint32_t AP_WINDOW_MS     = 30UL * 1000UL;   // one stable-config-AP window
static const uint32_t AP_CLIENT_HOLD_MS= 60UL * 1000UL;   // no hunt while / just after a client is on the AP
static const int      NO_PROGRESS_LIMIT= 5;               // hunt windows with no association -> reinitStack()

#ifdef IOTSA_WIFI_DEBUG
#define WCDEBUG(...) do { IotsaSerial.printf("iotsaWifi: " __VA_ARGS__); IotsaSerial.println(); } while (0)
#else
#define WCDEBUG(...) do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void IotsaWifiController::begin() {
  WCDEBUG("controller begin, radioEnabled=%d ssid='%s'", (int)_radioEnabled, _ssid.c_str());
  // First tick() does the initial reconcile.
}

void IotsaWifiController::tick() {
  IotsaWifiEvents ev = _driver.drainEvents();
  IotsaWifiActualState actual = _driver.readActualState();
  _apClientCount = actual.apClientCount;

  _handleEvents(ev, actual);
  _serviceTimers(actual);
  _reconcile(actual);
}

// ---------------------------------------------------------------------------
// Desired-state inputs
// ---------------------------------------------------------------------------

void IotsaWifiController::setCredentials(const String &ssid, const String &psk) {
  if (_ssid == ssid && _psk == psk) return;
  _ssid = ssid;
  _psk = psk;
  _cache.valid = false;             // a cached BSSID for a different network is useless
}

void IotsaWifiController::setRadioEnabled(bool on) { _radioEnabled = on; }
void IotsaWifiController::setConfigModeActive(bool active) { _configModeActive = active; }

void IotsaWifiController::credentialsChanged() {
  // A save of new credentials -> reconnect now, from a clean slate.
  WCDEBUG("credentialsChanged -> restart STA");
  if (_manualHunt) _leaveManualHunt();
  _staState = IotsaWifiStaState::Off;
  _huntGraceUsed = false;
  _noProgressHunts = 0;
  _takeoverDeadline.disarm();
  _dutyDeadline.disarm();
}

// ---------------------------------------------------------------------------
// Published state
// ---------------------------------------------------------------------------

IotsaWifiApState IotsaWifiController::apState() const {
  if (!_apUp) return IotsaWifiApState::Off;
  return _apDisruptionSafe() ? IotsaWifiApState::On : IotsaWifiApState::InUse;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

String IotsaWifiController::_apName() const {
  return String("config-") + iotsaConfig.hostName;
}

bool IotsaWifiController::_apDisruptionSafe() const {
  return _apClientCount == 0 && !_apClientHold.pending();
}

bool IotsaWifiController::_wantApUp() const {
  // Only consulted outside _manualHunt (the duty cycle owns the AP while hunting).
  if (!_radioEnabled) return false;
  if (_configModeActive) return true;                      // config mode => AP up alongside STA
  if (_ssid.length() == 0) return true;                    // unconfigured: offer the config AP
                                                           // (slice 4 folds this into "no ssid => config mode")
  return false;
}

void IotsaWifiController::_startStaAttempt() {
  uint8_t ch = 0;
  const uint8_t *bssid = nullptr;
  if (_cache.valid && _cache.ssid == _ssid) {
    ch = _cache.channel;
    bssid = _cache.bssid;
  }
  bool issued = _driver.startStation(_ssid, _psk, ch, bssid);
  WCDEBUG("startStation ssid='%s' targeted=%d issued=%d", _ssid.c_str(), (int)(bssid != nullptr), (int)issued);
  _staState = IotsaWifiStaState::Connecting;
}

void IotsaWifiController::_enterManualHunt() {
  WCDEBUG("takeover: SDK auto-reconnect not getting there -> manual hunt/AP duty cycle");
  _manualHunt = true;
  _huntGraceUsed = false;
  _noProgressHunts = 0;
  _takeoverDeadline.disarm();
  _driver.setAutoReconnect(false);   // it channel-hops and drags the softAP around

  // The AP may already be up (config mode / unconfigured raised it via _reconcile).
  IotsaWifiActualState a = _driver.readActualState();
  if (a.apEnabled && !_apDisruptionSafe()) {
    // A client is on it -- don't disrupt; start in an AP window instead.
    WCDEBUG("manual hunt: AP already in use -> start in AP window");
    _apUp = true;
    _dutyApPhase = true;
    _dutyDeadline.arm(AP_WINDOW_MS);
    return;
  }
  if (a.apEnabled) {                 // idle AP: clear it so the hunt scan doesn't drag it
    _driver.stopAP();
    _apUp = false;
  }
  _dutyApPhase = false;              // start in a hunt window
  _startStaAttempt();
  _dutyDeadline.arm(HUNT_WINDOW_MS);
}

void IotsaWifiController::_leaveManualHunt() {
  if (!_manualHunt) return;
  WCDEBUG("leaving manual hunt");
  _manualHunt = false;
  _dutyApPhase = false;
  _huntGraceUsed = false;
  _dutyDeadline.disarm();
  _driver.setAutoReconnect(true);
}

void IotsaWifiController::_handleEvents(const IotsaWifiEvents &ev, const IotsaWifiActualState &actual) {
  if (ev.staGotIp) {
    WCDEBUG("event: got IP, ch=%d", ev.lastChannel);
    if (_manualHunt) _leaveManualHunt();
    _staState = IotsaWifiStaState::Connected;
    _lastFailReason = IotsaWifiStaFailReason::None;
    _noProgressHunts = 0;
    _takeoverDeadline.disarm();
    _dutyDeadline.disarm();
    // refresh the fast-reconnect cache
    _cache.ssid = _ssid;
    _cache.channel = ev.lastChannel;
    memcpy(_cache.bssid, ev.lastBssid, 6);
    _cache.valid = (ev.lastChannel != 0);
  }
  if (ev.staFailed || ev.staLost) {
    _lastFailReason = ev.staFailed ? ev.staFailReason : IotsaWifiStaFailReason::Other;
    WCDEBUG("event: sta %s reason=%d", ev.staLost ? "lost" : "failed", (int)_lastFailReason);
    _staState = IotsaWifiStaState::Hunting;
    // Phase 1: let the SDK retry; give it TAKEOVER_MS before we step in. Arm once.
    if (!_manualHunt && !_takeoverDeadline.armed()) _takeoverDeadline.arm(TAKEOVER_MS);
  }
  if (ev.apClientCountChanged) {
    WCDEBUG("event: AP clients=%d", actual.apClientCount);
    if (actual.apClientCount > 0) _apClientHold.arm(AP_CLIENT_HOLD_MS);
  }
}

void IotsaWifiController::_serviceTimers(const IotsaWifiActualState &actual) {
  // Phase 1 -> 2: SDK auto-reconnect had its window and STA is still down.
  if (_takeoverDeadline.fired()) {
    const bool wantSta = _radioEnabled && _ssid.length() > 0;
    if (wantSta && _staState != IotsaWifiStaState::Connected) _enterManualHunt();
    return;
  }

  if (!_manualHunt || !_dutyDeadline.fired()) return;

  if (!_dutyApPhase) {
    // --- hunt window ended ---
    if (actual.staAssociated && !actual.staConnected && !_huntGraceUsed) {
      // A join is in flight (associated, DHCP pending) -- don't tear it down.
      WCDEBUG("hunt: associated, awaiting IP -> grace");
      _huntGraceUsed = true;
      _dutyDeadline.arm(HUNT_WINDOW_MS);
      return;
    }
    if (!actual.staAssociated) {
      if (++_noProgressHunts >= NO_PROGRESS_LIMIT) {
        WCDEBUG("stack wedged (%d dead hunt windows) -> reinitStack", _noProgressHunts);
        _driver.reinitStack();
        _noProgressHunts = 0;
      }
    } else {
      _noProgressHunts = 0;
    }
    WCDEBUG("hunt window end -> AP window");
    _driver.stopStation();
    if (_driver.startAP(_apName())) _apUp = true;
    _dutyApPhase = true;
    _huntGraceUsed = false;
    _dutyDeadline.arm(AP_WINDOW_MS);
  } else {
    // --- AP window ended ---
    if (!_apDisruptionSafe()) {
      // Someone is using the config AP (or just left) -- keep it, skip this hunt.
      WCDEBUG("AP window end, AP in use -> extend");
      _dutyDeadline.arm(AP_WINDOW_MS);
      return;
    }
    WCDEBUG("AP window end -> hunt window");
    _driver.stopAP();
    _apUp = false;
    _dutyApPhase = false;
    _startStaAttempt();
    _dutyDeadline.arm(HUNT_WINDOW_MS);
  }
}

void IotsaWifiController::_reconcile(const IotsaWifiActualState &actual) {
  const bool wantSta = _radioEnabled && _ssid.length() > 0;

  if (_manualHunt) {
    if (!wantSta) {
      // Radio disabled or credentials cleared out from under us.
      WCDEBUG("reconcile: STA no longer wanted -> leave manual hunt");
      _driver.stopStation();
      if (actual.apEnabled && _apDisruptionSafe()) { _driver.stopAP(); }
      _leaveManualHunt();
      _staState = IotsaWifiStaState::Off;
      _apUp = actual.apEnabled;
      return;
    }
    // The duty cycle in _serviceTimers() owns both radios while hunting.
    _apUp = actual.apEnabled;
    return;
  }

  // ---- STA (non-manual-hunt) ----
  if (!wantSta) {
    _takeoverDeadline.disarm();
    if (actual.staEnabled) {
      WCDEBUG("reconcile: STA not wanted -> stopStation");
      _driver.stopStation();
    }
    _staState = IotsaWifiStaState::Off;
  } else {
    switch (_staState) {
      case IotsaWifiStaState::Off:
        _startStaAttempt();
        break;
      case IotsaWifiStaState::Connecting:
        // Only bail early if the radio is demonstrably pursuing the wrong SSID
        // (credentials changed under us mid-attempt).
        if (actual.staEnabled &&
            actual.staConfiguredSsid.length() > 0 &&
            actual.staConfiguredSsid != _ssid) {
          WCDEBUG("reconcile: radio pursuing stale SSID -> restart");
          _startStaAttempt();
        }
        break;
      case IotsaWifiStaState::Hunting:
        // SDK auto-reconnect is retrying; wait for _takeoverDeadline.
        break;
      case IotsaWifiStaState::Connected:
        // setAutoReconnect(true) covers a transient blip; a real loss arrives as
        // ev.staLost. Nothing to do here.
        break;
    }
  }

  // ---- AP (derived fact, non-manual-hunt) ----
  const bool wantAp = _wantApUp();
  if (wantAp && !actual.apEnabled) {
    if (_driver.startAP(_apName())) {
      WCDEBUG("reconcile: AP up ('%s')", _apName().c_str());
      _apUp = true;
    }
  } else if (!wantAp && actual.apEnabled) {
    if (_apDisruptionSafe()) {
      WCDEBUG("reconcile: AP down");
      _driver.stopAP();
      _apUp = false;
    }
    // else: keep it until the client leaves / the hold expires (docs "AP stability")
  } else {
    _apUp = actual.apEnabled;
  }
}

#endif // IOTSA_WITH_WIFI
