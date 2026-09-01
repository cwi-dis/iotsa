#ifndef _IOTSAWIFICONTROLLER_H_
#define _IOTSAWIFICONTROLLER_H_
#include <Arduino.h>
#include "iotsaDeadline.h"

//
// The WiFi *policy* layer -- see docs/wifi-controller-design.md.
//
// IotsaWifiMod is the driver (radio ops + latched events + introspection) plus
// module glue plus persisted settings. IotsaWifiController is policy only: the STA
// state machine, the retry/escalation cadence, the derived "AP should be up" fact,
// and a declare-desired / reconcile / safe-to-act loop. It has no IotsaModule-ness,
// no persistence, no REST. IotsaWifiMod owns one as a member, feeds it the desired
// state, and reads back the published state (which IotsaWifiMod then copies into
// the iotsaConfig fields other modules read).
//

class IotsaWifiMod; // controller calls back into the driver methods on this

// ---------------------------------------------------------------------------
// Shared driver <-> controller value types (free, not nested, to avoid an
// iotsaWifi.h <-> iotsaWifiController.h circular include).
// ---------------------------------------------------------------------------

// Mixed-case members throughout this file: several obvious SCREAMING_CASE names
// (DISABLED, CONNECTED, ...) are framework macros and collide even in an enum class.
enum class IotsaWifiStaFailReason : uint8_t { None, NoApFound, AuthFail, Other };

struct IotsaWifiActualState {
  bool staEnabled = false;      // WiFi.getMode() has the STA bit
  bool apEnabled = false;       // WiFi.getMode() has the AP bit
  bool staConnected = false;    // WiFi.status() == WL_CONNECTED
  int staLinkStatus = 0;        // raw wl_status_t
  String staConfiguredSsid;     // the target, readable even mid-connect
  String staConfiguredPsk;
  uint8_t staChannel = 0;       // meaningful only while staConnected
  String apSsid;
  int apClientCount = 0;
};

struct IotsaWifiEvents {        // what the driver's callbacks latched since last drain
  bool staGotIp = false;
  bool staFailed = false;
  IotsaWifiStaFailReason staFailReason = IotsaWifiStaFailReason::None;
  bool staLost = false;         // was connected, then dropped
  bool apClientCountChanged = false;
  uint8_t lastChannel = 0;      // from the most recent staGotIp
  uint8_t lastBssid[6] = {0};   // from the most recent staGotIp
};

// The two WiFi modes' states, published to iotsaConfig. Each enum just reports
// its own mode; interesting *combinations* (fallback AP, config-mode AP, "setup
// succeeded but still in config mode") are derived at the call site by combining
// these two with iotsaConfig.inConfigurationMode() -- see docs.
enum class IotsaWifiStaState : uint8_t {
  Off,          // not attempting (no credentials, or wifiDisabledOnBoot)
  Connecting,   // connect issued, nothing wrong yet
  Connected,    // has an IP
  Hunting       // a connect attempt failed; retrying on a per-reason cadence
};
enum class IotsaWifiApState : uint8_t {
  Off,          // no softAP
  On,           // softAP up, no client using it -- safe to disrupt (channel-hop scan, rename)
  InUse         // a client is associated (or within the post-disconnect hold):
                // do not channel-hop or restart the AP; this is also the
                // "radioShouldStayCalm" contribution (docs)
};

// ---------------------------------------------------------------------------

class IotsaWifiController {
public:
  explicit IotsaWifiController(IotsaWifiMod &mod) : _mod(mod) {}

  // Lifecycle, driven by IotsaWifiMod.
  void begin();  // from IotsaWifiMod::setup(), after configLoad()
  void tick();   // from IotsaWifiMod::loop()

  // ---- desired-state inputs (pushed by IotsaWifiMod) ----
  void setCredentials(const String &ssid, const String &psk);
  void setRadioEnabled(bool on);          // false when wifiDisabledOnBoot
  void setConfigModeActive(bool active);  // from iotsaConfig.inConfigurationMode(), each tick
  void credentialsChanged();              // putHandler just saved new creds -> reconnect promptly

  // ---- published state (read by IotsaWifiMod, copied to iotsaConfig) ----
  IotsaWifiStaState staState() const { return _staState; }
  IotsaWifiApState apState() const;   // Off/On/InUse, computed from _apUp + client count + hold
  bool staConnected() const { return _staState == IotsaWifiStaState::Connected; }  // convenience
  bool apActive() const { return apState() != IotsaWifiApState::Off; }             // convenience
  IotsaWifiStaFailReason lastFailReason() const { return _lastFailReason; }
  // ms until the config AP is raised while STA is failing; 0 if not counting / already up
  uint32_t escalationRemainingMillis() const { return _escalationDeadline.remainingMillis(); }

private:
  void _handleEvents(const IotsaWifiEvents &ev, const IotsaWifiActualState &actual);
  void _serviceTimers();
  void _reconcile(const IotsaWifiActualState &actual);   // desired vs actual, act if safe
  void _startStaAttempt();
  bool _wantApUp() const;                                // the derived AP-up rule
  uint32_t _retryDelayMillis() const;                    // per-reason HUNTING cadence
  bool _apDisruptionSafe() const;                        // no client + hold expired
  String _apName() const;                                // "config-<hostname>"

  IotsaWifiMod &_mod;

  // desired state
  String _ssid, _psk;
  bool _radioEnabled = true;
  bool _configModeActive = false;

  // current state
  IotsaWifiStaState _staState = IotsaWifiStaState::Off;
  bool _apUp = false;              // policy says the softAP should exist; apState() adds On vs InUse
  bool _escalated = false;         // STA failed long enough -> AP-up derivation is true
  IotsaWifiStaFailReason _lastFailReason = IotsaWifiStaFailReason::None;
  int _apClientCount = 0;          // last known softAP client count
  int _retryCount = 0;            // consecutive HUNTING attempts (for AUTH_FAIL backoff)
  int _noProgressAttempts = 0;    // connects that never even reached the radio -> reinitStack()

  // RTC-RAM fast-reconnect cache (docs "Fast-reconnect cache"): filled on GOT_IP,
  // used for a targeted scan-free reconnect. Not persisted -- regenerated cheaply
  // after a cold boot.
  struct FastReconnect {
    String ssid;
    uint8_t bssid[6] = {0};
    uint8_t channel = 0;
    bool valid = false;
  } _cache;

  // One IotsaDeadline per purpose (docs "Scheduler primitive").
  IotsaDeadline _connectDeadline;    // STA connect attempt timeout
  IotsaDeadline _retryDeadline;      // gap before the next HUNTING attempt
  IotsaDeadline _escalationDeadline; // STA failing -> raise the config AP after this
  IotsaDeadline _apClientHold;       // no channel-hopping scan while a client is on the AP
};

#endif
