#include "iotsa.h"
#include "iotsaController.h"
#if defined(ESP32) && ESP_ARDUINO_VERSION_MAJOR > 2
#include "rom/ets_sys.h"   // ets_printf, for the watchdog ISR
#endif

//
// Global variable definition
//
IotsaController iotsaController;

// The mode machine and radio/sleep policy moved into their own objects
// (cwi-dis/iotsa#106 step 5a). IotsaController is now: seed + tick the
// sub-policies, the deferred-reboot timer, and the hardware watchdog (5d).

#ifdef ESP32
// One hardware-timer watchdog, module-static (only IotsaController touches it).
static hw_timer_t *s_watchdog = nullptr;

static void IRAM_ATTR watchdogFired() {
  ets_printf("iotsa watchdog reboot");
  esp_restart();
}

void IotsaController::rearmWatchdog() {
  uint32_t ms = iotsaConfig.watchdogDuration;
  if (s_watchdog) {
#if ESP_ARDUINO_VERSION_MAJOR <= 2
    timerAlarmDisable(s_watchdog);
    timerDetachInterrupt(s_watchdog);
#else
    timerDetachInterrupt(s_watchdog);
#endif
    timerEnd(s_watchdog);
    s_watchdog = nullptr;
  }
  if (ms == 0) return;
#if ESP_ARDUINO_VERSION_MAJOR <= 2
  s_watchdog = timerBegin(0, 80, true);                 // 80 -> 1 MHz (1 tick = 1 us)
  timerAttachInterrupt(s_watchdog, &watchdogFired, true);
  timerAlarmWrite(s_watchdog, ms * 1000, false);
  timerAlarmEnable(s_watchdog);
#else
  s_watchdog = timerBegin(1000000);                     // 1 MHz
  timerAttachInterrupt(s_watchdog, &watchdogFired);
  timerAlarm(s_watchdog, ms * 1000, true, 0);
#endif
  IFDEBUG IotsaSerial.printf("iotsaController: watchdog %u ms\n", (unsigned)ms);
}

void IotsaController::_feedWatchdog() {
  if (s_watchdog) timerWrite(s_watchdog, 0);
}

void IotsaController::pauseWatchdog() {
  if (!s_watchdog) return;
#if ESP_ARDUINO_VERSION_MAJOR <= 2
  timerAlarmDisable(s_watchdog);
#else
  timerDetachInterrupt(s_watchdog);
#endif
}

void IotsaController::resumeWatchdog() {
  if (!s_watchdog) return;
  timerWrite(s_watchdog, 0);
#if ESP_ARDUINO_VERSION_MAJOR <= 2
  timerAlarmEnable(s_watchdog);
#else
  timerAttachInterrupt(s_watchdog, &watchdogFired);
  timerAlarm(s_watchdog, iotsaConfig.watchdogDuration * 1000, true, 0);
#endif
}
#endif // ESP32

void IotsaController::begin() {
  _radio.seedFromBootPolicy(
    iotsaConfig.wifiDisabledOnBoot,
#ifdef IOTSA_WITH_BLE
    iotsaConfig.bleDisabledOnBoot
#else
    true   // no BLE -> "disabled on boot" is vacuously true
#endif
  );
  _modes.begin(iotsaStatus.wasHardwareReset());
#ifdef ESP32
  rearmWatchdog();
#endif
}

void IotsaController::tick() {
#ifdef ESP32
  _feedWatchdog();
#endif
  if (_rebootAtMillis && millis() > _rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    ESP.restart();
  }
  _modes.tick();
}

void IotsaController::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  _rebootAtMillis = millis() + ms;
}
