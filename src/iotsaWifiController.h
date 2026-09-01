#ifndef _IOTSAWIFICONTROLLER_H_
#define _IOTSAWIFICONTROLLER_H_
#include <Arduino.h>
#include "iotsaDeadline.h"
#include "iotsaWifiDriver.h"   // IotsaWifiDriver + the shared value types

//
// The WiFi *policy* layer -- see docs/wifi-controller-design.md.
//
// IotsaWifiDriver is the mechanism (radio ops + latched events + introspection).
// IotsaWifiController is policy only: the STA state machine, the retry/escalation
// cadence, the derived "AP should be up" fact, and a declare-desired / reconcile /
// safe-to-act loop. No IotsaModule-ness, no persistence, no REST. IotsaWifiMod
// owns both, feeds this one its desired state, and reads back the published state
// (which IotsaWifiMod then copies into the iotsaConfig fields other modules read).
//

// Mixed-case enum members throughout: several obvious SCREAMING_CASE names
// (DISABLED, CONNECTED, ...) are framework macros and collide even in an enum class.

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
  explicit IotsaWifiController(IotsaWifiDriver &driver) : _driver(driver) {}

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

  IotsaWifiDriver &_driver;

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
