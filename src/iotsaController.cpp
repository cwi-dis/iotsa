#include "iotsa.h"
#include "iotsaController.h"
#ifdef ESP32
#include <esp_task_wdt.h>
#endif

//
// Global variable definition
//
IotsaController iotsaController;

// The mode machine and radio/sleep policy moved into their own objects
// (cwi-dis/iotsa#106 step 5a). IotsaController is now: seed + tick the
// sub-policies, the deferred-reboot timer, and the watchdog (5d, cwi-dis/iotsa#244).

#if defined(ESP32) && !defined(IOTSA_WITHOUT_WATCHDOG)
// ESP-IDF's task watchdog, on the loop task. The Arduino core has already
// initialised it (5 s, panic on, watching the idle task(s)); we only change the
// timeout and subscribe the loop task. On a timeout ESP-IDF panics: the reset
// reason is ESP_RST_TASK_WDT and the serial log names the task.
static TaskHandle_t s_watchdogTask = nullptr;   // the loop task, once subscribed
static bool s_watchdogPaused = false;

void IotsaController::_startWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config = {
    .timeout_ms = WATCHDOG_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  config.idle_core_mask |= 1 << 0;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  config.idle_core_mask |= 1 << 1;
#endif
  if (esp_task_wdt_reconfigure(&config) != ESP_OK) esp_task_wdt_init(&config);
#else
  // ESP-IDF 4: init on an initialised watchdog only changes timeout and panic.
  esp_task_wdt_init(WATCHDOG_SECONDS, true);
#endif
  TaskHandle_t self = xTaskGetCurrentTaskHandle();
  if (esp_task_wdt_add(self) == ESP_OK) {
    s_watchdogTask = self;
    IOTSA_LOG("iotsaController", "watchdog %u s", (unsigned)WATCHDOG_SECONDS);
  } else {
    IOTSA_LOG("iotsaController", "watchdog could not be started");
  }
}

void IotsaController::feedWatchdog() {
  // Only the subscribed task can feed it (and a feed from e.g. the NimBLE task
  // shouldn't count anyway: it's loop() we're watching).
  if (s_watchdogTask && !s_watchdogPaused && xTaskGetCurrentTaskHandle() == s_watchdogTask) {
    esp_task_wdt_reset();
  }
}

void IotsaController::pauseWatchdog() {
  if (!s_watchdogTask || s_watchdogPaused) return;
  esp_task_wdt_delete(s_watchdogTask);
  s_watchdogPaused = true;
}

void IotsaController::resumeWatchdog() {
  if (!s_watchdogTask || !s_watchdogPaused) return;
  esp_task_wdt_add(s_watchdogTask);
  s_watchdogPaused = false;
}

#elif !defined(ESP32)
// ESP8266: the core's watchdog is on from boot, fed by every return from
// loop(), yield() and delay(), and can't be switched off: stopping only the
// software one would let the hardware one fire after ~6 s instead of ~3 s. So
// IOTSA_WITHOUT_WATCHDOG has no effect here. Sleep is either deep sleep (a
// reset) or delay(), so pause/resume have nothing to do.
void IotsaController::_startWatchdog() {}
void IotsaController::feedWatchdog() { ESP.wdtFeed(); }
void IotsaController::pauseWatchdog() {}
void IotsaController::resumeWatchdog() {}

#else
// ESP32 with IOTSA_WITHOUT_WATCHDOG.
void IotsaController::_startWatchdog() {}
void IotsaController::feedWatchdog() {}
void IotsaController::pauseWatchdog() {}
void IotsaController::resumeWatchdog() {}
#endif

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
  _startWatchdog();
}

void IotsaController::tick() {
  feedWatchdog();
  if (_rebootAtMillis && millis() > _rebootAtMillis) {
    IFDEBUG IotsaSerial.println("Software requested reboot.");
    iotsaBreadcrumbs.addBreadcrumb(IOTSA_CRUMB_REBOOT);
    ESP.restart();
  }
  _modes.tick();
}

void IotsaController::requestReboot(uint32_t ms) {
  IFDEBUG IotsaSerial.println("Restart requested");
  _rebootAtMillis = millis() + ms;
}
