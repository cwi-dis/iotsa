#ifndef _IOTSAWIFICONTROLLER_H_
#define _IOTSAWIFICONTROLLER_H_
#include <Arduino.h>
#include "iotsaDeadline.h"
#include "iotsaWifiDriver.h"   // IotsaWifiDriver + the shared value types

//
// The WiFi *policy* layer -- see docs/wifi-controller-design.md.
//
// IotsaWifiDriver is the mechanism (radio ops + latched events + introspection).
// IotsaWifiController is policy only: the STA state machine, the fallback-AP /
// hunt duty cycle, and a declare-desired / reconcile / safe-to-act loop. No
// IotsaModule-ness, no persistence, no REST. IotsaWifiMod owns both, feeds this
// one its desired state, and reads back the published state (which IotsaWifiMod
// then copies into the iotsaConfig fields other modules read).
//
// STA connect strategy, in two phases:
//   1. Normal: SDK auto-reconnect is ON -- it handles a fast initial connect and
//      transient blips. The controller just watches.
//   2. Takeover: if STA stays down for TAKEOVER_MS, the SDK is clearly not
//      getting there. The controller turns auto-reconnect OFF (it channel-hops
//      and drags the softAP around) and runs its own duty cycle: HUNT_WINDOW_MS
//      of STA hunting with the AP down, then AP_WINDOW_MS of a stable, joinable
//      config AP with STA quiet, repeat. A client on the AP forestalls the next
//      hunt. got-IP at any point returns to phase 1.
//

// Mixed-case enum members throughout: several obvious SCREAMING_CASE names
// (DISABLED, CONNECTED, ...) are framework macros and collide even in an enum class.

// The two WiFi modes' states, published to iotsaConfig. Each enum just reports
// its own mode; interesting *combinations* (fallback AP, config-mode AP, "setup
// succeeded but still in config mode") are derived at the call site by combining
// these two with iotsaController.inConfigurationMode() -- see docs.
enum class IotsaWifiStaState : uint8_t {
  Off,          // not attempting (no credentials, or wifiDisabledOnBoot)
  Connecting,   // connect issued, nothing wrong yet
  Connected,    // has an IP
  Hunting       // a connect attempt failed; SDK auto-reconnect or the duty cycle is retrying
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
  void setConfigModeActive(bool active);  // from iotsaController.inConfigurationMode(), each tick
  void credentialsChanged();              // putHandler just saved new creds -> reconnect promptly

  // ---- published state (read by IotsaWifiMod, copied to iotsaConfig) ----
  IotsaWifiStaState staState() const { return _staState; }
  IotsaWifiApState apState() const;   // Off/On/InUse, computed from _apUp + client count + hold
  bool staConnected() const { return _staState == IotsaWifiStaState::Connected; }  // convenience
  bool apActive() const { return apState() != IotsaWifiApState::Off; }             // convenience
  IotsaWifiStaFailReason lastFailReason() const { return _lastFailReason; }
  bool inManualHunt() const { return _manualHunt; }   // running the AP/hunt duty cycle

private:
  void _handleEvents(const IotsaWifiEvents &ev, const IotsaWifiActualState &actual);
  void _serviceTimers(const IotsaWifiActualState &actual);   // takeover + duty-cycle phase transitions
  void _reconcile(const IotsaWifiActualState &actual);       // desired vs actual, act if safe
  void _startStaAttempt();
  void _enterManualHunt();
  void _leaveManualHunt();
  bool _wantApUp() const;                                // AP-up rule for the non-manual-hunt case
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
  IotsaWifiStaFailReason _lastFailReason = IotsaWifiStaFailReason::None;
  int _apClientCount = 0;          // last known softAP client count

  // Duty cycle (phase 2). _manualHunt: SDK auto-reconnect off, we own both radios.
  bool _manualHunt = false;
  bool _dutyApPhase = false;       // within _manualHunt: true = AP window, false = hunt window
  bool _huntGraceUsed = false;     // one grace extension per hunt window (mid-DHCP protection)
  int _noProgressHunts = 0;        // hunt windows that produced no association -> reinitStack()

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
  IotsaDeadline _takeoverDeadline;  // STA down this long with SDK auto-reconnect -> _enterManualHunt()
  IotsaDeadline _dutyDeadline;      // current duty-cycle phase (hunt window / AP window / grace)
  IotsaDeadline _apClientHold;      // no channel-hopping scan while / just after a client is on the AP
};

#endif
