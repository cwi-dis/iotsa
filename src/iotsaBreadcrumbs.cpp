#include "iotsa.h"
#include "iotsaBreadcrumbs.h"

//
// Global variable definitions
//
IotsaBreadcrumbs iotsaBreadcrumbs;
bool iotsaBreadcrumbsEnabled = true;

//
// Storage: 32 words of RTC memory.
//   [0]      header: 16-bit magic, 8 bits spare, 8-bit index of the next ring write
//   [1]      activity word
//   [2..31]  event ring
//
// ESP8266: RTC user memory words 96-127. User memory starts at 0x60001200, the
// same address as the eboot OTA command block (eboot_command.h), so its first
// 128 bytes (words 0-31) belong to eboot. Accessed directly rather than through
// ESP.rtcUserMemoryWrite(): one 32-bit store per write.
// ESP32: RTC memory that the startup code leaves alone (RTC_NOINIT_ATTR).
//
#ifdef ESP32
static RTC_NOINIT_ATTR uint32_t s_rtcWords[32];
#define RTC_WORDS ((volatile uint32_t *)s_rtcWords)
#else
#define RTC_WORDS ((volatile uint32_t *)(0x60001200 + 96 * 4))
#endif

static constexpr uint32_t MAGIC = 0xb5c7;
static constexpr int HEADER = 0;
static constexpr int ACTIVITY = 1;
static constexpr int RING = 2;

static inline uint32_t makeEntry(uint8_t code, uint8_t arg) {
  return ((uint32_t)code << 24) | ((uint32_t)arg << 16) | ((millis() >> 16) & 0xffff);
}

void IotsaBreadcrumbs::begin() {
  // Not affected by iotsaBreadcrumbsEnabled: the boot record is written once,
  // and it is what makes the rest worth having.
  uint8_t cause = iotsaStatus.rawBootReason();
  uint32_t header = RTC_WORDS[HEADER];
  if ((header >> 16) != MAGIC) {
    // Power-on, or never initialised: the contents are garbage.
    for (int i = 0; i < 32; i++) RTC_WORDS[i] = 0;
    RTC_WORDS[HEADER] = MAGIC << 16;
  } else {
    // Reset with RTC memory intact: keep the history, and record what we were
    // doing (with its uptime) before the reset.
    uint32_t previous = RTC_WORDS[ACTIVITY];
    if (previous >> 24) {
      _append(previous);
      const char *name = codeName(entryCode(previous));
      if (name) {
        IOTSA_LOG("iotsaBreadcrumbs", "before this reset: %s %u, uptime %u s", name, entryArg(previous), (unsigned)entryUptime(previous));
      } else {
        IOTSA_LOG("iotsaBreadcrumbs", "before this reset: code %u %u, uptime %u s", entryCode(previous), entryArg(previous), (unsigned)entryUptime(previous));
      }
    }
  }
  _append(makeEntry(IOTSA_CRUMB_BOOT, cause));
  RTC_WORDS[ACTIVITY] = makeEntry(IOTSA_CRUMB_BOOTING, 0);
}

void IotsaBreadcrumbs::setActivity(uint8_t code, uint8_t arg) {
  if (!iotsaBreadcrumbsEnabled) return;
  RTC_WORDS[ACTIVITY] = makeEntry(code, arg);
}

uint32_t IotsaBreadcrumbs::activity() {
  return RTC_WORDS[ACTIVITY];
}

void IotsaBreadcrumbs::restoreActivity(uint32_t word) {
  setActivity(entryCode(word), entryArg(word));
}

void IotsaBreadcrumbs::addBreadcrumb(uint8_t code, uint8_t arg) {
  if (!iotsaBreadcrumbsEnabled) return;
  _append(makeEntry(code, arg));
}

void IotsaBreadcrumbs::_append(uint32_t entry) {
  // Entry first, then the index, so a reset in between loses at most this entry.
  // Events are rare, so two tasks appending at the same moment isn't guarded against.
  uint32_t header = RTC_WORDS[HEADER];
  uint32_t index = header & 0xff;
  if (index >= RING_SIZE) index = 0;
  RTC_WORDS[RING + index] = entry;
  index = (index + 1) % RING_SIZE;
  RTC_WORDS[HEADER] = (header & ~0xffu) | index;
}

int IotsaBreadcrumbs::events(uint32_t *entries) {
  uint32_t index = RTC_WORDS[HEADER] & 0xff;
  if (index >= RING_SIZE) index = 0;
  int count = 0;
  for (int i = 0; i < RING_SIZE; i++) {
    uint32_t entry = RTC_WORDS[RING + (index + i) % RING_SIZE];
    if (entry >> 24) entries[count++] = entry;   // unused slots are 0
  }
  return count;
}

const char *IotsaBreadcrumbs::codeName(uint8_t code) {
  switch (code) {
    case IOTSA_CRUMB_BOOTING: return "booting";
    case IOTSA_CRUMB_LOOP: return "loop";
    case IOTSA_CRUMB_CORE: return "core";
    case IOTSA_CRUMB_REQUEST: return "request";
    case IOTSA_CRUMB_OTA: return "ota";
    case IOTSA_CRUMB_BLE_CALLBACK: return "bleCallback";
    case IOTSA_CRUMB_SLEEP: return "sleep";
    case IOTSA_CRUMB_BOOT: return "boot";
    case IOTSA_CRUMB_REBOOT: return "reboot";
    case IOTSA_CRUMB_FACTORY_RESET: return "factoryReset";
    default: return nullptr;
  }
}
