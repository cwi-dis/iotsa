#ifndef _IOTSADEADLINE_H_
#define _IOTSADEADLINE_H_
#include <Arduino.h>

//
// A single millis()-based one-shot deadline with uniform semantics, replacing the
// assorted hand-rolled `wantXxxAtMillis` / `xxxTimeoutMillis` scalars scattered
// through iotsaWifi, iotsaBattery, iotsaBLEClient and iotsaConfig (each with
// slightly different "is it 0? is it in the past? do I clear it?" conventions).
//
// A state machine / reconcile loop instantiates one per purpose (connect deadline,
// retry cadence, escalation delay, AP-client hold, ...) and checks them in its tick.
//
// millis() rollover safe: comparisons use signed difference, correct as long as a
// deadline is less than ~24 days out (always true for these uses).
//
class IotsaDeadline {
public:
  // Arm to fire `fromNowMillis` from now. Re-arming an already-armed deadline just
  // moves it.
  void arm(uint32_t fromNowMillis) {
    _dueAtMillis = millis() + fromNowMillis;
    _armed = true;
  }
  void disarm() { _armed = false; }
  bool armed() const { return _armed; }

  // True while armed and past due; stays true until disarm() (level-triggered).
  // Use this when the reconcile loop wants to keep acting on the expiry every tick.
  bool expired() const {
    return _armed && (int32_t)(millis() - _dueAtMillis) >= 0;
  }

  // True while armed and NOT yet due -- i.e. a countdown is in progress. The
  // natural "is this hold/backoff currently in effect" test.
  bool pending() const {
    return _armed && (int32_t)(millis() - _dueAtMillis) < 0;
  }

  // expired() with consume: returns true at most once per arm(), disarming itself.
  // Use this for "did this deadline just fire" edge handling.
  bool fired() {
    if (!expired()) return false;
    _armed = false;
    return true;
  }

  // Milliseconds left until due; 0 if disarmed or already expired. For status /
  // info display ("times out in N s") and for sizing the next tick interval.
  uint32_t remainingMillis() const {
    if (!_armed) return 0;
    int32_t left = (int32_t)(_dueAtMillis - millis());
    return left > 0 ? (uint32_t)left : 0;
  }

private:
  uint32_t _dueAtMillis = 0;
  bool _armed = false;
};

#endif
