#include "iotsa.h"
#include "iotsaStatus.h"
#include "iotsaController.h"   // statusColor() reads iotsaController.currentMode()/requestedMode()
#include <math.h>               // fabsf() (breatheEnvelope())
#ifdef ESP32
#include <esp_log.h>
#include <rom/rtc.h>
#endif

//
// Global variable definition
//
IotsaStatus iotsaStatus;

static constexpr uint32_t SLOT_MS = 1000;              // one mode/wifi slot
static constexpr uint32_t GAP_MS = 200;                // dark gap between slots
static constexpr uint32_t CYCLE_MS = 2 * SLOT_MS + 2 * GAP_MS;
static constexpr uint32_t SLOW_BLINK_PERIOD_MS = 500;  // 2 Hz
static constexpr uint32_t FAST_BLINK_PERIOD_MS = 250;  // 4 Hz

// Colour (+ reason, for non-LED renderers) for a *requested* mode -- the
// modal-override case (cwi-dis/iotsa#176 design comment, section 3).
static uint32_t colourForRequestedMode(iotsa_mode mode, const char **reasonOut) {
  switch (mode) {
    case IOTSA_MODE_CONFIG:
      *reasonOut = "Configuration requested (press reset)";
      return IotsaStatus::COLOUR_MAGENTA;
    case IOTSA_MODE_OTA:
      *reasonOut = "OTA requested (press reset)";
      return IotsaStatus::COLOUR_CYAN;
    case IOTSA_MODE_FACTORY_RESET:
      *reasonOut = "Factory reset requested (press reset)";
      return IotsaStatus::COLOUR_WHITE;
    default:
      *reasonOut = nullptr;
      return 0;
  }
}

// Rendering primitives -- pure functions of (rhythm-relative time, period).
// Kept file-local: only statusColor() needs pixel-level envelope math: other
// renderers consume the semantic IotsaStatusSignal accessors instead.
static float breatheEnvelope(uint32_t phaseMs, uint32_t periodMs) {
  if (periodMs == 0) return 1.0f;
  float x = (float)(phaseMs % periodMs) / (float)periodMs;  // 0..1
  return 1.0f - fabsf(2.0f * x - 1.0f);                      // triangle: 0..1..0
}

static bool blinkOn(uint32_t phaseMs, uint32_t periodMs) {
  if (periodMs == 0) return true;
  return (phaseMs % periodMs) < (periodMs / 2);
}

static uint32_t scaleColour(uint32_t colour, float factor) {
  uint8_t r = (uint8_t)(((colour >> 16) & 0xff) * factor);
  uint8_t g = (uint8_t)(((colour >> 8) & 0xff) * factor);
  uint8_t b = (uint8_t)((colour & 0xff) * factor);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Renders one semantic signal to a 0xRRGGBB tint at rhythm-relative time t
// (a slot-local phase for the slot cycle, or plain millis() for a
// continuous/non-slotted rhythm -- blink/breathe are periodic in t either way).
static uint32_t renderSignal(const IotsaStatusSignal &sig, uint32_t t) {
  if (sig.colour == 0) return 0;
  switch (sig.rhythm) {
    case IotsaStatusRhythm::Dark:      return 0;
    case IotsaStatusRhythm::Breathe:   return scaleColour(sig.colour, breatheEnvelope(t, SLOT_MS));
    case IotsaStatusRhythm::SlowBlink: return blinkOn(t, SLOW_BLINK_PERIOD_MS) ? sig.colour : 0;
    case IotsaStatusRhythm::FastBlink: return blinkOn(t, FAST_BLINK_PERIOD_MS) ? sig.colour : 0;
    case IotsaStatusRhythm::Solid:     return sig.colour;
  }
  return 0;
}

bool IotsaStatus::networkIsUp() {
  return wifiStationConnected;
}

const char* IotsaStatus::getBootReason() {
  static const char *reason = NULL;
  if (reason == NULL) {
    reason = "unknown";
#ifndef ESP32
    rst_info *rip = ESP.getResetInfoPtr();
    static const char *reasons[] = {
      "power",
      "hardwareWatchdog",
      "exception",
      "softwareWatchdog",
      "softwareReboot",
      "deepSleepAwake",
      "externalReset"
    };
    if (rip->reason < sizeof(reasons)/sizeof(reasons[0])) {
      reason = reasons[(int)rip->reason];
    }
#else
#if 1
    esp_reset_reason_t r = esp_reset_reason();
    switch(r) {
      case ESP_RST_UNKNOWN: reason = "unknown"; break;
      case ESP_RST_POWERON: reason = "power"; break;
      case ESP_RST_EXT: reason = "externalReset"; break;
      case ESP_RST_SW: reason = "softwareReboot"; break;
      case ESP_RST_PANIC: reason = "panic"; break;
      case ESP_RST_INT_WDT: reason = "interruptWatchdog"; break;
      case ESP_RST_TASK_WDT: reason = "taskWatchdog"; break;
      case ESP_RST_WDT: reason = "hardwareWatchdog"; break;
      case ESP_RST_DEEPSLEEP: reason = "deepSleepAwake"; break;
      case ESP_RST_BROWNOUT: reason = "brownout"; break;
      case ESP_RST_SDIO: reason = "sdioReset"; break;
      default: reason = "other"; break;
    }
#else
  RESET_REASON r1 = rtc_get_reset_reason(0);
  RESET_REASON r2 = rtc_get_reset_reason(1);
  static char reasonBuffer[64];
  // Determine best reset reason
  static const char *reasons[] = {
    "0",
    "power",
    "2",
    "softwareReboot",
    "legacyWatchdog",
    "deepSleepAwake",
    "sdio",
    "tg0Watchdog",
    "tg1Watchdog",
    "rtcWatchdog",
    "intrusion",
    "tgWatchdogCpu",
    "softwareRebootCpu",
    "rtcWatchdogCpu",
    "externalReset",
    "brownout",
    "rtcWatchdogRtc"
  };
  if ((int)r1 < sizeof(reasons)/sizeof(reasons[0])) {
    strcpy(reasonBuffer, reasons[(int)r1]);
  }
  strcpy(reasonBuffer + strlen(reasonBuffer), "/");
  if ((int)r2 < sizeof(reasons)/sizeof(reasons[0])) {
    strcat(reasonBuffer, reasons[(int)r2]);
  }
  reason = reasonBuffer;
#endif
#endif
  }
  return reason;
}

bool IotsaStatus::wasHardwareReset() {
  // The anti-tamper gate for honouring a pending mode request (cwi-dis/iotsa#106):
  // only a real power-cycle or reset-button press counts, never a software reboot,
  // watchdog or crash.
#ifndef ESP32
  rst_info *rip = ESP.getResetInfoPtr();
  return rip->reason == REASON_DEFAULT_RST || rip->reason == REASON_EXT_SYS_RST;
#else
  // xxxjack Not sure why I sometimes see the WDT reset on pressing the reset button...
  RESET_REASON r = rtc_get_reset_reason(0);
  return r == POWERON_RESET || r == RTCWDT_RTC_RESET;
#endif
}

void IotsaStatus::printHeapSpace() {
  // Difficult to print on esp8266. Debugging only, so just don't print anything.
#ifdef ESP32
  size_t memAvail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  IFDEBUG IotsaSerial.printf("Time since boot: %lld ms. Available heap space: %u bytes, largest block: %u bytes\n", (int64_t)millis(), memAvail, largestBlock);
#endif
}

IotsaStatusSignal IotsaStatus::overrideSignal() const {
  IotsaStatusSignal sig;
  if (millis() < _pulseExpiryMs) {
    sig.colour = _pulseColour;
    sig.rhythm = (_pulseOffMs == 0) ? IotsaStatusRhythm::Solid : IotsaStatusRhythm::FastBlink;
    sig.reason = _pulseReason;
    return sig;
  }
  iotsa_mode requested = iotsaController.requestedMode();
  if (requested != IOTSA_MODE_NORMAL) {
    sig.colour = colourForRequestedMode(requested, &sig.reason);
    sig.rhythm = IotsaStatusRhythm::FastBlink;
  }
  return sig;
}

IotsaStatusSignal IotsaStatus::modeSignal() const {
  IotsaStatusSignal sig;
  iotsa_mode mode = iotsaController.currentMode();
  // IOTSA_MODE_FACTORY_RESET is never seen here: factoryReset() runs
  // synchronously on mode entry and reboots before loop() runs again
  // (cwi-dis/iotsa#176 design comment, section 3).
  if (mode == IOTSA_MODE_CONFIG) {
    sig.colour = IotsaStatus::COLOUR_MAGENTA;
    sig.rhythm = IotsaStatusRhythm::Breathe;
  } else if (mode == IOTSA_MODE_OTA) {
    sig.colour = IotsaStatus::COLOUR_CYAN;
    sig.rhythm = IotsaStatusRhythm::Breathe;
  }
  // IOTSA_MODE_NORMAL: dark (default sig).
  return sig;
}

IotsaStatusSignal IotsaStatus::wifiSignal() const {
  IotsaStatusSignal sig;
  if (!wifiEnabled) return sig;  // radio disabled: dark
  sig.colour = IotsaStatus::COLOUR_AMBER;
  // Breathe whenever the AP is actually reachable right now -- config mode and
  // "unconfigured" both hold it up persistently via _wantApUp(), but that's
  // deliberately "the non-manual-hunt case" (its own doc comment): it says
  // nothing about the manual-hunt duty cycle's periodic AP window, which is
  // just as much "the way in" while it's open. Keying off wifiApActive itself
  // covers all three sources in one go instead of re-deriving _wantApUp()'s
  // (incomplete, for this purpose) two conditions. Confirmed missing on real
  // hardware (cwi-dis/iotsa#176 hardware pass): the LED stayed slow-blink
  // through the duty cycle's AP window instead of breathing.
  //
  // Within that, a client actually connected to the AP (wifiApInUse) gets
  // Solid instead of Breathe -- otherwise "someone is on my config AP right
  // now" is visually identical to "AP is up, nobody's there", which a second
  // hardware pass showed to be a genuine gap: joining the AP produced no
  // visible change at all (cwi-dis/iotsa#176).
  if (wifiApActive) {
    sig.rhythm = wifiApInUse ? IotsaStatusRhythm::Solid : IotsaStatusRhythm::Breathe;
  } else if (wifiHunting) {
    sig.rhythm = IotsaStatusRhythm::SlowBlink;
  } else {
    sig.colour = 0;  // connected, nothing unusual: dark
  }
  return sig;
}

void IotsaStatus::setStatusPulse(uint32_t colour, uint32_t onMs, uint32_t offMs, uint32_t durationMs, const char *reason) {
  _pulseColour = colour;
  _pulseOnMs = onMs;
  _pulseOffMs = offMs;
  _pulseStartMs = millis();
  _pulseExpiryMs = _pulseStartMs + durationMs;
  _pulseReason = reason;
}

void IotsaStatus::clearStatusPulse() {
  _pulseExpiryMs = 0;
}

uint32_t IotsaStatus::statusColor() {
  // Precedence (cwi-dis/iotsa#176 design comment, section 4): pulse channel >
  // modal override (a pending mode request) > the two-slot mode/wifi cycle.
  // Rendered here with exact pulse on/off timing (overrideSignal() buckets
  // any pulse to Solid/FastBlink for non-LED renderers, which don't need
  // millisecond-exact timing).
  uint32_t now = millis();
  if (now < _pulseExpiryMs) {
    if (_pulseOffMs == 0) return _pulseColour;  // solid
    uint32_t t = (now - _pulseStartMs) % (_pulseOnMs + _pulseOffMs);
    return (t < _pulseOnMs) ? _pulseColour : 0;
  }

  iotsa_mode requested = iotsaController.requestedMode();
  if (requested != IOTSA_MODE_NORMAL) {
    const char *reason;
    uint32_t colour = colourForRequestedMode(requested, &reason);
    return blinkOn(now, FAST_BLINK_PERIOD_MS) ? colour : 0;
  }

  // Two-slot cycle: [mode][gap][wifi][gap], repeating every CYCLE_MS.
  uint32_t phase = now % CYCLE_MS;
  if (phase < SLOT_MS) return renderSignal(modeSignal(), phase);
  phase -= SLOT_MS;
  if (phase < GAP_MS) return 0;
  phase -= GAP_MS;
  if (phase < SLOT_MS) return renderSignal(wifiSignal(), phase);
  return 0;  // second gap
}
