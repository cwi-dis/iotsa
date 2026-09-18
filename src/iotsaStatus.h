#ifndef _IOTSASTATUS_H_
#define _IOTSASTATUS_H_

#include <stdint.h>   // uint32_t (statusColor())

// Intended to be included from iotsa.h

//
// IotsaStatus -- the read-mostly status board (cwi-dis/iotsa#106).
//
// Volatile, runtime-derived facts about what the device is doing right now:
// written (mostly every tick, by the controllers) and read by everyone. The
// opposite lifecycle to IotsaConfig, which holds identity -- persisted, and
// written only during a maintenance window. A plain data struct: no module, no
// loop(), no lifecycle. See docs/controller-architecture.md.
//
// Being split out of IotsaConfig incrementally; iotsaConfig keeps deprecated
// forwarders for one release (docs "Transition strategy").
//

// Rhythm vocabulary for the status indicator (cwi-dis/iotsa#176): urgency /
// what's expected of the user, independent of colour (which says *which*
// subsystem). Dark/Solid have no envelope; Breathe/SlowBlink/FastBlink do --
// see the (private, .cpp-internal) breatheEnvelope()/blinkOn() helpers.
enum class IotsaStatusRhythm : uint8_t { Dark, Breathe, SlowBlink, FastBlink, Solid };

// A semantic status fact: *what* is being signalled, with no time-math or RGB
// envelope applied yet. Deliberately renderer-agnostic -- statusColor() below
// renders one onto a single NeoPixel, but a future non-LED renderer (a web
// page, a sound pattern, ...) can consume these directly instead. colour==0
// means "nothing to show" (same sentinel convention as statusColor()'s 0).
// reason is a human-readable "why", only ever set on a signal coming from an
// active pulse or a pending mode request -- the plain mode/wifi facts don't
// need one, colour+rhythm already says everything there is to say.
struct IotsaStatusSignal {
  uint32_t colour = 0;
  IotsaStatusRhythm rhythm = IotsaStatusRhythm::Dark;
  const char *reason = nullptr;
};

class IotsaStatus {
public:
  // Status-indicator colours (cwi-dis/iotsa#176): amber=wifi, magenta=config,
  // cyan=OTA, white=generic/blank-slate. One home so setStatusPulse() callers
  // elsewhere in the framework don't hardcode hex. Levels reuse the existing
  // dim scale (0x3f per channel). Red/green are reserved for the application /
  // remote-command layer (IotsaLedMod::set()) -- deliberately not listed here.
  static constexpr uint32_t COLOUR_AMBER   = 0x3f1f00;
  static constexpr uint32_t COLOUR_MAGENTA = 0x3f003f;
  static constexpr uint32_t COLOUR_CYAN    = 0x003f3f;
  static constexpr uint32_t COLOUR_WHITE   = 0x3f3f3f;

  bool wifiEnabled = false;           // WiFi radio is not disabled (NOT "connected" -- see networkIsUp())
  bool wifiStationConnected = false;  // STA has an IP
  bool wifiApActive = false;          // softAP is up, for any reason
  bool wifiApInUse = false;           // ...and a client is actually connected to it (published by IotsaWifiMod)
  bool wifiConfigured = false;        // an SSID is configured (published by IotsaWifiMod)
  bool wifiHunting = false;           // STA connect attempt failed, SDK/duty-cycle retry in progress (published by IotsaWifiMod)
  bool mdnsEnabled = false;           // mDNS responder is running
  bool onUsbPower = false;            // running on USB power (published by IotsaBatteryMod; false if no VUSB sense)

  bool networkIsUp();                 // reachable over the configured WiFi network (STA has an IP)
  const char *getBootReason();        // human-readable reset cause (computed once, then cached)
  bool wasHardwareReset();            // this boot was a power-cycle / reset-button, not a software reboot / watchdog / crash
  void printHeapSpace();              // debug: free heap + largest block (prints on ESP32 only)

  // ---- semantic layer (cwi-dis/iotsa#176) ----
  // "What's going on" -- no time-math, no RGB. See IotsaStatusSignal above.
  IotsaStatusSignal overrideSignal() const;  // active pulse, else a pending mode request, else colour=0
  IotsaStatusSignal modeSignal() const;      // currentMode() as a colour+rhythm fact (dark in normal mode)
  IotsaStatusSignal wifiSignal() const;      // wifi state as a colour+rhythm fact (always amber or dark)

  // ---- human-readable summary (cwi-dis/iotsa#176) ----
  // One line of plain text (no HTML) for the web interface's "Status:" line, built from
  // the same facts /api/status reports: mode (+ time left), the station and access-point
  // radios as two independent clauses, a pending mode request, else the current notice.
  // Only defined in IOTSA_WITH_WEB builds.
  String statusText() const;

  // ---- pulse channel (cwi-dis/iotsa#176) ----
  // Transient "show colour for duration", e.g. an OTA chunk arriving or a
  // config write landing. Last poke wins; falls back to the mode-request /
  // slot-cycle signal automatically once it expires -- no restore logic.
  // reason is stored as a raw pointer: pass only string literals / statics.
  void setStatusPulse(uint32_t colour, uint32_t onMs, uint32_t offMs, uint32_t durationMs, const char *reason = nullptr);
  void clearStatusPulse();

  // ---- single-NeoPixel renderer (cwi-dis/iotsa#176) ----
  // Renders overrideSignal() / modeSignal() / wifiSignal() (the latter two
  // time-multiplexed onto a shared cycle, since one pixel can only show one
  // colour at a time) to a 0xRRGGBB tint, breathe/blink envelope already
  // applied. 0 = "indicator off". Consumed by IotsaLedMod; app displays with
  // more than one pixel (e.g. iotsaNeoClock) are expected to use the semantic
  // accessors above directly instead. Moved off IotsaConfig in cwi-dis/iotsa#243.
  uint32_t statusColor();

private:
  uint32_t _pulseColour = 0;
  uint32_t _pulseOnMs = 0;
  uint32_t _pulseOffMs = 0;
  uint32_t _pulseStartMs = 0;
  uint32_t _pulseExpiryMs = 0;   // 0 = no active pulse (mirrors iotsaLed.cpp's nextChangeTime==0 sentinel)
  const char *_pulseReason = nullptr;
};

extern IotsaStatus iotsaStatus;
#endif
