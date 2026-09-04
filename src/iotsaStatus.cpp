#include "iotsa.h"
#include "iotsaStatus.h"
#include "iotsaController.h"   // statusColor() reads iotsaController.currentMode()
#ifdef ESP32
#include <esp_log.h>
#include <rom/rtc.h>
#endif

//
// Global variable definition
//
IotsaStatus iotsaStatus;

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

uint32_t IotsaStatus::statusColor() {
  // The old wifiMode switch, translated onto the iotsaStatus bus (cwi-dis/iotsa#106);
  // lived on IotsaConfig until cwi-dis/iotsa#243. currentMode stays owned by
  // IotsaController -- we read through to it. The real LED-semantics rework (flash
  // patterns, etc.) is cwi-dis/iotsa#176.
  iotsa_mode mode = iotsaController.currentMode();
  if (mode == IOTSA_MODE_FACTORY_RESET) return 0x3f0000;   // Red: factory-reset mode
  if (!wifiEnabled) return 0;                               // radio disabled: LED off

  uint32_t extraColor = 0;
  if (!wifiStationConnected) {
    if (wifiApActive) {
      extraColor = 0x1f1f1f;      // white tint: serving our own AP (fallback / unconfigured)
    } else {
      return 0x3f1f00;           // Orange: hunting for WiFi
    }
  }
  if (mode == IOTSA_MODE_CONFIG) return extraColor | 0x3f003f;  // Magenta: configuration mode
  if (mode == IOTSA_MODE_OTA)    return extraColor | 0x003f3f;  // Cyan: OTA mode
  return extraColor; // Off when connected+normal; whiteish on the fallback AP
}
