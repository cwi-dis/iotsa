#ifndef _IOTSAWIFIDRIVER_H_
#define _IOTSAWIFIDRIVER_H_
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

//
// IotsaWifiDriver -- the WiFi *mechanism* layer (cwi-dis/iotsa#106).
//
// A thin, policy-free wrapper over the platform WiFi API: imperative radio ops,
// the events its callbacks latch, and introspection. No timers, no retry logic,
// no state machine, no writes to iotsaConfig. IotsaWifiController drives it;
// IotsaWifiMod owns both and feeds the driver its one setting (tx-power
// reduction). Not an IotsaBaseModule -- just a plain object.
//

// Mixed-case enum members: DISABLED / CONNECTED / ... are framework macros and
// collide even inside an enum class.
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

struct IotsaWifiEvents {        // what the callbacks latched since the last drain
  bool staGotIp = false;
  bool staFailed = false;
  IotsaWifiStaFailReason staFailReason = IotsaWifiStaFailReason::None;
  bool staLost = false;         // was connected, then dropped
  bool apClientCountChanged = false;
  uint8_t lastChannel = 0;      // from the most recent staGotIp
  uint8_t lastBssid[6] = {0};   // from the most recent staGotIp
};

class IotsaWifiDriver {
public:
  void begin();                 // install the platform WiFi event handlers (once)
  void setTxPowerReduction(bool on) { _txPowerReduction = on; }

  // Radio ops: fire, report whether the *attempt* was issued (not whether it
  // completed).
  bool startStation(const String &ssid, const String &psk, uint8_t channel = 0, const uint8_t *bssid = nullptr);
  void stopStation();
  bool startAP(const String &apName);
  void stopAP();
  void reinitStack();           // disconnect(true)/mode(OFF)/mode(STA) -- unwedge, never reboot

  IotsaWifiActualState readActualState() const;
  IotsaWifiEvents drainEvents();  // read-and-clear the latched events

private:
  static IotsaWifiStaFailReason _reduceStaFailReason(int reason);

  bool _txPowerReduction = false;
  bool _handlersInstalled = false;

  // Latch storage, written only from the platform WiFi callbacks (foreign task
  // context) -- single-word writes only, see cwi-dis/iotsa#236.
  volatile bool _evStaGotIp = false;
  volatile bool _evStaFailed = false;
  volatile uint8_t _evStaFailReason = 0;
  volatile bool _evStaLost = false;
  volatile bool _evApClientCountChanged = false;
  volatile uint8_t _evLastChannel = 0;
  uint8_t _evLastBssid[6] = {0};
  bool _haveIp = false;          // touched only in the callbacks: staLost vs staFailed
#ifndef ESP32
  WiFiEventHandler _evH_gotIp, _evH_disconnected, _evH_apConnect, _evH_apDisconnect;
#endif
};

#endif
