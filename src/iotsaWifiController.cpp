#include "iotsaWifiController.h"
#include "iotsa.h"
#include "iotsaWifi.h"

#ifdef IOTSA_WITH_WIFI

//
// IotsaWifiController -- the WiFi policy layer. See docs/wifi-controller-design.md.
// IotsaWifiMod owns one, feeds it desired state (setCredentials / setRadioEnabled /
// setConfigModeActive) each tick, and reads back staState() / apState() to publish
// into iotsaConfig. The controller drives the radio only through the driver methods
// on IotsaWifiMod (startStation / stopStation / startAP / stopAP / reinitStack /
// readActualState / drainEvents).
//

// Tunables (docs "Open questions" -- to be adjusted against real hardware).
static const uint32_t CONNECT_TIMEOUT_MS   = IOTSA_WIFI_TIMEOUT * 1000UL; // per STA connect attempt
static const uint32_t ESCALATION_MS        = 60UL * 1000UL;              // STA failing -> raise the config AP
static const uint32_t RETRY_BASE_MS        = 15UL * 1000UL;              // gap between HUNTING attempts (NoApFound/Other)
static const uint32_t RETRY_AUTHFAIL_MS    = 30UL * 1000UL;              // first AUTH_FAIL retry gap (then doubles)
static const uint32_t RETRY_CAP_MS         = 15UL * 60UL * 1000UL;       // longest retry gap
static const uint32_t AP_CLIENT_HOLD_MS    = 60UL * 1000UL;              // no channel-hop scan while / after a client
static const int      NO_PROGRESS_LIMIT    = 5;                          // dead connects before reinitStack()

#ifdef IOTSA_WIFI_DEBUG
#define WCDEBUG(...) do { IotsaSerial.printf("iotsaWifi: " __VA_ARGS__); IotsaSerial.println(); } while (0)
#else
#define WCDEBUG(...) do {} while (0)
#endif

static const char *_staName(IotsaWifiStaState s) {
  switch (s) {
    case IotsaWifiStaState::Off:        return "Off";
    case IotsaWifiStaState::Connecting: return "Connecting";
    case IotsaWifiStaState::Connected:  return "Connected";
    case IotsaWifiStaState::Hunting:    return "Hunting";
  }
  return "?";
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void IotsaWifiController::begin() {
  WCDEBUG("controller begin, radioEnabled=%d ssid='%s'", (int)_radioEnabled, _ssid.c_str());
  // First tick() does the initial reconcile.
}

void IotsaWifiController::tick() {
  IotsaWifiEvents ev = _mod.drainEvents();
  IotsaWifiActualState actual = _mod.readActualState();
  _apClientCount = actual.apClientCount;

  _handleEvents(ev, actual);
  _serviceTimers();
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
  // A save of new credentials -> reconnect now, don't wait out a retry gap.
  WCDEBUG("credentialsChanged -> restart STA");
  _staState = IotsaWifiStaState::Off;
  _escalated = false;
  _retryCount = 0;
  _noProgressAttempts = 0;
  _connectDeadline.disarm();
  _retryDeadline.disarm();
  _escalationDeadline.disarm();
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
  if (!_radioEnabled) return false;
  if (_configModeActive) return true;                      // config mode => AP always up
  if (_ssid.length() == 0) return true;                    // unconfigured: offer the config AP
                                                           // (slice 4 folds this into "no ssid => config mode")
  if (_staState == IotsaWifiStaState::Hunting && _escalated) return true; // fallback
  return false;
}

uint32_t IotsaWifiController::_retryDelayMillis() const {
  if (_lastFailReason == IotsaWifiStaFailReason::AuthFail) {
    // back off hard -- hammering a failed auth gets rate-limited / temp-banned
    uint32_t d = RETRY_AUTHFAIL_MS << (_retryCount > 6 ? 6 : _retryCount);
    return d > RETRY_CAP_MS ? RETRY_CAP_MS : d;
  }
  // NoApFound / Other: steady, stretching out after a long spell
  uint32_t d = RETRY_BASE_MS + (uint32_t)(_retryCount > 20 ? 20 : _retryCount) * RETRY_BASE_MS;
  return d > RETRY_CAP_MS ? RETRY_CAP_MS : d;
}

void IotsaWifiController::_startStaAttempt() {
  uint8_t ch = 0;
  const uint8_t *bssid = nullptr;
  if (_cache.valid && _cache.ssid == _ssid) {
    ch = _cache.channel;
    bssid = _cache.bssid;
  }
  bool issued = _mod.startStation(_ssid, _psk, ch, bssid);
  WCDEBUG("startStation ssid='%s' targeted=%d issued=%d", _ssid.c_str(), (int)(bssid != nullptr), (int)issued);
  if (!issued) {
    // WiFi.begin() outright refused -- treat like a connect failure, will retry.
    _staState = IotsaWifiStaState::Hunting;
    _lastFailReason = IotsaWifiStaFailReason::Other;
    _retryDeadline.arm(_retryDelayMillis());
    if (!_escalationDeadline.armed()) _escalationDeadline.arm(ESCALATION_MS);
    if (++_noProgressAttempts >= NO_PROGRESS_LIMIT) {
      WCDEBUG("stack wedged -> reinitStack");
      _mod.reinitStack();
      _noProgressAttempts = 0;
    }
    return;
  }
  _noProgressAttempts = 0;
  _staState = IotsaWifiStaState::Connecting;
  _connectDeadline.arm(CONNECT_TIMEOUT_MS);
  _retryDeadline.disarm();
}

void IotsaWifiController::_handleEvents(const IotsaWifiEvents &ev, const IotsaWifiActualState &actual) {
  if (ev.staGotIp) {
    WCDEBUG("event: got IP, ch=%d", ev.lastChannel);
    _staState = IotsaWifiStaState::Connected;
    _lastFailReason = IotsaWifiStaFailReason::None;
    _escalated = false;
    _retryCount = 0;
    _noProgressAttempts = 0;
    _connectDeadline.disarm();
    _retryDeadline.disarm();
    _escalationDeadline.disarm();
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
    _connectDeadline.disarm();
    _retryDeadline.arm(_retryDelayMillis());
    if (!_escalationDeadline.armed()) _escalationDeadline.arm(ESCALATION_MS);
  }
  if (ev.apClientCountChanged) {
    WCDEBUG("event: AP clients=%d", actual.apClientCount);
    if (actual.apClientCount > 0) _apClientHold.arm(AP_CLIENT_HOLD_MS);
  }
}

void IotsaWifiController::_serviceTimers() {
  if (_connectDeadline.fired()) {
    // No got-ip and no disconnect event within the window -- silent timeout.
    WCDEBUG("connect timeout");
    _lastFailReason = IotsaWifiStaFailReason::Other;
    _staState = IotsaWifiStaState::Hunting;
    _retryDeadline.arm(_retryDelayMillis());
    if (!_escalationDeadline.armed()) _escalationDeadline.arm(ESCALATION_MS);
    if (++_noProgressAttempts >= NO_PROGRESS_LIMIT) {
      WCDEBUG("stack wedged -> reinitStack");
      _mod.reinitStack();
      _noProgressAttempts = 0;
    }
  }
  if (_escalationDeadline.fired()) {
    WCDEBUG("escalation: raising config AP");
    _escalated = true;
  }
  // _retryDeadline is consumed in _reconcile() (it gates the next attempt).
}

void IotsaWifiController::_reconcile(const IotsaWifiActualState &actual) {
  const bool wantSta = _radioEnabled && _ssid.length() > 0;

  // ---- STA ----
  if (!wantSta) {
    if (actual.staEnabled) {
      WCDEBUG("reconcile: STA not wanted -> stopStation");
      _mod.stopStation();
    }
    if (_staState != IotsaWifiStaState::Off) _staState = IotsaWifiStaState::Off;
  } else {
    switch (_staState) {
      case IotsaWifiStaState::Off:
        _startStaAttempt();
        break;
      case IotsaWifiStaState::Hunting:
        if (!_retryDeadline.pending()) {   // gap elapsed (or never armed)
          _retryCount++;
          _startStaAttempt();
        }
        break;
      case IotsaWifiStaState::Connecting:
        // Trust _connectDeadline (and the fail/got-ip events) to move us on --
        // don't second-guess a connect in progress from a transient link status.
        // Only bail early if the radio is demonstrably pursuing the wrong SSID
        // (credentials changed under us mid-attempt).
        if (actual.staEnabled &&
            actual.staConfiguredSsid.length() > 0 &&
            actual.staConfiguredSsid != _ssid) {
          WCDEBUG("reconcile: radio pursuing stale SSID -> restart");
          _startStaAttempt();
        }
        break;
      case IotsaWifiStaState::Connected:
        // setAutoReconnect(true) covers a transient blip; a real loss arrives as
        // ev.staLost. Nothing to do here.
        break;
    }
  }

  // ---- AP (derived fact) ----
  const bool wantAp = _wantApUp();
  if (wantAp && !actual.apEnabled) {
    // Bringing the AP up is always safe (there is no client on an AP that is off).
    if (_mod.startAP(_apName())) {
      WCDEBUG("reconcile: AP up ('%s')", _apName().c_str());
      _apUp = true;
    }
  } else if (!wantAp && actual.apEnabled) {
    if (_apDisruptionSafe()) {
      WCDEBUG("reconcile: AP down");
      _mod.stopAP();
      _apUp = false;
    }
    // else: keep it until the client leaves / the hold expires (docs "AP stability")
  } else {
    _apUp = actual.apEnabled;
  }
}

#endif // IOTSA_WITH_WIFI
