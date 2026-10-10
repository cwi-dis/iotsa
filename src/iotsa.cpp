#include <Esp.h>
#include "iotsa.h"
#include "iotsaHttpServer.h"
#include "iotsaConfigMod.h"
#include "iotsaRunmode.h"
#include "iotsaWifi.h"
#include "iotsaOta.h"
#include "iotsaLed.h"
#include "iotsaFS.h"
#if defined(IOTSA_HAS_COAPSERVER) || defined(IOTSA_HAS_HPSSERVER)
#include "iotsaApi.h"
#endif
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#endif

// There is an issue with the platformio library dependency finder, and it doesn't find the
// esp8266httpclient library. This is a workaround.
#include "iotsaRequest.h"

// Initialize IotsaSerial (a define) to refer to the normal Serial.
// Will be overridden if the iotsaLogger module is included.
Print *iotsaOverrideSerial = &Serial;
bool iotsaLogDebugEnabled = true;  // see iotsaLog.h; set from IotsaRunmodeMod's config

#ifdef ESP32
static TaskHandle_t s_loopTask = nullptr;   // set in setup(), which runs in the loop task
#endif

IotsaApplication::IotsaApplication(const char *_title)
: firstModule(NULL),
  firstEarlyModule(NULL),
  title(_title)
{
  // Unlike the CoAP/HPS companion mods, the HTTP transport can't be created lazily
  // on first use by whichever module happens to need it -- several categories of
  // module (web-server-extension modules, IotsaApiServiceWeb/Rest) reach for it
  // during their own construction, see cwi-dis/iotsa#207/#211. Ensuring it here
  // relies on the existing convention that IotsaApplication itself is declared
  // before any module in the sketch.
#ifdef IOTSA_HAS_WEBSERVER
  server = IotsaHttpServiceMod::ensure(*this)->server;
#endif
}

// Default when the application has no auth provider: everything is allowed.
class IotsaAllowAllAuth : public IotsaAuthenticationProvider {
public:
  bool allows(const char *right=NULL) override { return true; }
  bool allows(const char *obj, IotsaApiOperation verb) override { return true; }
};
static IotsaAllowAllAuth iotsaAllowAllAuth;

IotsaAuthenticationProvider *
IotsaApplication::getAuth() {
  return _auth ? _auth : &iotsaAllowAllAuth;
}

void
IotsaApplication::addMod(IotsaBaseModule *mod) {
  mod->nextModule = firstModule;
  firstModule = mod;
}

void
IotsaApplication::addModEarly(IotsaBaseModule *mod) {
  mod->nextModule = firstEarlyModule;
  firstEarlyModule = mod;
}

void
IotsaApplication::setup() {
  // xxxjack Unsure about this. We always open the Serial port,
  // so log messages that aren't flagged with IFDEBUG always work.
  // But this means the serial port cannot be used for other things.
  Serial.begin(IOTSA_SERIAL_SPEED);
  iotsaBreadcrumbs.begin();
#ifdef ESP32
  s_loopTask = xTaskGetCurrentTaskHandle();
#endif
  IFDEBUG IotsaSerial.println("Serial opened");
  // Always shown, not IFDEBUG-gated: "which firmware is this, exactly" is the first
  // thing you want on a cold boot, not something to enable after the fact.
  IotsaSerial.print("iotsa ");
  IotsaSerial.print(IOTSA_FULL_VERSION);
#ifdef IOTSA_CONFIG_PROGRAM_NAME
  IotsaSerial.print(" / ");
  IotsaSerial.print(IOTSA_CONFIG_PROGRAM_NAME);
#endif
#ifdef IOTSA_CONFIG_PROGRAM_VERSION
  IotsaSerial.print(" ");
  IotsaSerial.print(IOTSA_CONFIG_PROGRAM_VERSION);
#endif
  IotsaSerial.println();
#ifdef IOTSA_DELAY_ON_BOOT
  IFDEBUG IotsaSerial.printf("Delaying %d seconds on boot...\n", IOTSA_DELAY_ON_BOOT);
  delay(IOTSA_DELAY_ON_BOOT*1000);
  IFDEBUG IotsaSerial.printf("Delayed %d seconds on boot...\n", IOTSA_DELAY_ON_BOOT);
#endif
  IFDEBUG IotsaSerial.print("Opening " IOTSA_FS_NAME " (may take long)...");
  bool ok = IOTSA_FS.begin();
  IFDEBUG IotsaSerial.println(" done.");
  if (!ok) {
    IFDEBUG IotsaSerial.println("IOTSA_FS.begin() failed, formatting");

    ok = IOTSA_FS.format();
    if (!ok) {
      IFDEBUG IotsaSerial.println(IOTSA_FS_NAME ".format() failed");
    }
    ok = IOTSA_FS.begin();
    if (!ok) {
      IFDEBUG IotsaSerial.println(IOTSA_FS_NAME ".begin() after format failed");
    }
  } else {
    IFDEBUG IotsaSerial.println(IOTSA_FS_NAME " mounted");
  }
  iotsaConfig.ensureConfigLoaded();
  // Consume the pending-mode mailbox and run the boot anti-tamper gate before any
  // module's setup() sees the mode (cwi-dis/iotsa#106). Was in IotsaConfigMod::setup().
  iotsaController.begin();

  // IotsaConfigMod (hostname, TLS certs, configuration-mode handling) is core
  // infrastructure, independent of WiFi. It used to be created only as a member of
  // IotsaWifiMod, so a WiFi-less build lost /api/config entirely (cwi-dis/iotsa#195).
  // Ensure it here; an explicit declaration or IotsaWifiMod (which forwards its auth
  // provider) still wins via the singleton.
  IotsaConfigMod::ensure(*this);

  // IotsaRunmodeMod (the control surface onto IotsaController: mode requests,
  // reboot, runtime radio toggles) is core-tier too, same treatment as
  // IotsaConfigMod -- see docs/controller-architecture.md (cwi-dis/iotsa#106).
  IotsaRunmodeMod::ensure(*this);

  // WiFi and OTA are standard modules too: created here unless the sketch
  // declared them itself (cwi-dis/iotsa#85). Opt out at build time with
  // -DIOTSA_WITHOUT_WIFI / -DIOTSA_WITHOUT_OTA.
#ifdef IOTSA_WITH_WIFI
  IotsaWifiMod::ensure(*this);
#endif
#ifdef IOTSA_WITH_OTA
  IotsaOtaMod::ensure(*this);
#endif
  // The status LED, when the board definition says there is one (cwi-dis/iotsa#272).
#if defined(IOTSA_PIN_LED) && !defined(IOTSA_WITHOUT_STATUS_LED)
  IotsaLedMod::ensure(*this);
#endif

  // Ensure the CoAP/HPS companion modules exist before any module's setup() runs,
  // rather than being lazily created as a side effect of whichever module happens to
  // construct an IotsaApiServiceCoap/Hps member first (see cwi-dis/iotsa#113 case 3).
#ifdef IOTSA_HAS_COAPSERVER
  IotsaApiServiceCoap::ensureServiceMod(*this);
#endif
#ifdef IOTSA_HAS_HPSSERVER
  IotsaApiServiceHps::ensureServiceMod(*this);
#endif
#ifdef IOTSA_WITH_BLE
  // Any BLE service (HPS, battery, an app's own) is registered against the one
  // IotsaBLEServerMod, and only its lateSetupDone() actually starts advertising --
  // so without an instance the GATT services exist but are never announced. Declaring
  // the module in the sketch was easy to forget (see cwi-dis/iotsa#84); guarantee it
  // here instead. Gated on IOTSA_WITH_BLE for now -- when BLE-client-only builds
  // become real this moves to a server-role flag (see #84's discussion).
  IotsaBLEServerMod::ensure(*this);
#endif

  IotsaBaseModule *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->setup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->setup();
  }
  IFDEBUG IotsaSerial.print("hostname: ");
  IFDEBUG IotsaSerial.println(iotsaConfig.hostName);
  // Easy mistake when converting an old sketch (cwi-dis/iotsa#284): removing the
  // auth argument from the module constructors without adding setAuth() leaves
  // everything unprotected.
  if (_haveAuthModule && _auth == nullptr) {
    IotsaSerial.println("IOTSA: WARNING: authentication module present, but application.setAuth() not called: nothing is protected");
  }
}

void
IotsaApplication::lateSetup() {
  IotsaBaseModule *m;

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->lateSetup();
  }

  for (m=firstModule; m; m=m->nextModule) {
  	m->lateSetup();
  }

  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->lateSetupDone();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->lateSetupDone();
  }
}

//
// Work handed to the loop task from other tasks (cwi-dis/iotsa#236). One queue,
// drained at the start of every loop() pass. A runInLoop() caller waits on its
// entry's semaphore; on a timeout it removes the entry if it hasn't started,
// or keeps waiting if it has (fn may reference the caller's stack).
//
#ifdef ESP32
#include <deque>
#include <mutex>
#endif

struct IotsaPostedWork {
  std::function<void()> fn;
#ifdef ESP32
  SemaphoreHandle_t done = nullptr;   // runInLoop() only: the waiter owns the entry
  bool started = false;
#endif
};

static constexpr size_t POSTED_QUEUE_MAX = 32;
#ifdef ESP32
static std::deque<IotsaPostedWork *> s_posted;
static std::mutex s_postedMutex;
#else
static std::vector<IotsaPostedWork *> s_posted;
#endif

bool
IotsaApplication::inLoopTask() {
#ifdef ESP32
  return s_loopTask != nullptr && xTaskGetCurrentTaskHandle() == s_loopTask;
#else
  return true;
#endif
}

bool
IotsaApplication::postToLoop(std::function<void()> fn) {
#ifdef ESP32
  std::lock_guard<std::mutex> lock(s_postedMutex);
#endif
  if (s_posted.size() >= POSTED_QUEUE_MAX) return false;
  IotsaPostedWork *w = new IotsaPostedWork;
  w->fn = std::move(fn);
  s_posted.push_back(w);
  return true;
}

bool
IotsaApplication::runInLoop(std::function<void()> fn, uint32_t timeoutMs) {
#ifdef ESP32
  if (inLoopTask() || s_loopTask == nullptr) {
    fn();
    return true;
  }
  IotsaPostedWork *w = new IotsaPostedWork;
  w->fn = std::move(fn);
  w->done = xSemaphoreCreateBinary();
  {
    std::lock_guard<std::mutex> lock(s_postedMutex);
    if (s_posted.size() >= POSTED_QUEUE_MAX) {
      vSemaphoreDelete(w->done);
      delete w;
      return false;
    }
    s_posted.push_back(w);
  }
  bool ok = xSemaphoreTake(w->done, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  if (!ok) {
    std::unique_lock<std::mutex> lock(s_postedMutex);
    if (!w->started) {
      for (auto it = s_posted.begin(); it != s_posted.end(); ++it) {
        if (*it == w) { s_posted.erase(it); break; }
      }
      lock.unlock();
      vSemaphoreDelete(w->done);
      delete w;
      return false;
    }
    // Already running: it must finish before we may return.
    lock.unlock();
    xSemaphoreTake(w->done, portMAX_DELAY);
  }
  vSemaphoreDelete(w->done);
  delete w;
  return true;
#else
  fn();
  return true;
#endif
}

void
IotsaApplication::_runPosted() {
  // Only what is queued now: work posted while draining waits for the next pass.
  size_t count;
  {
#ifdef ESP32
    std::lock_guard<std::mutex> lock(s_postedMutex);
#endif
    count = s_posted.size();
  }
  while (count-- > 0) {
    IotsaPostedWork *w;
    {
#ifdef ESP32
      std::lock_guard<std::mutex> lock(s_postedMutex);
#endif
      if (s_posted.empty()) return;
      w = s_posted.front();
      s_posted.erase(s_posted.begin());
#ifdef ESP32
      w->started = true;
#endif
    }
    iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_POSTED);
    w->fn();
#ifdef ESP32
    if (w->done) {
      xSemaphoreGive(w->done);   // the waiter deletes w; don't touch it any more
      continue;
    }
#endif
    delete w;
  }
}

void
IotsaApplication::_loopModules(IotsaBaseModule *skip) {
  IotsaBaseModule *m;
  uint8_t index = 0;
  for (m=firstEarlyModule; m; m=m->nextModule, index++) {
    if (m == skip) continue;
    iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_LOOP, index);
    _loopingModule = m;
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule, index++) {
    if (m == skip) continue;
    iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_LOOP, index);
    _loopingModule = m;
  	m->loop();
  }
}

void
IotsaApplication::aboutToBlock() {
  if (_inAboutToBlock) return;
  if (!inLoopTask()) return;
  _inAboutToBlock = true;
  IotsaBaseModule *caller = _loopingModule;
  uint32_t activity = iotsaBreadcrumbs.activity();
  iotsaController.tick();
  _loopModules(caller);
  _loopingModule = caller;
  iotsaBreadcrumbs.restoreActivity(activity);
  _inAboutToBlock = false;
}

void
IotsaApplication::loop() {
  iotsaController.tick();
  _runPosted();
  _loopModules(nullptr);
  _loopingModule = nullptr;
  iotsaBreadcrumbs.setActivity(IOTSA_CRUMB_CORE);
#ifdef ESP32
  {
    // Print available free heap space first time we have gone through all loop() calls.
    static bool once = false;
    if (!once) {
      iotsaStatus.printHeapSpace();
      once = true;
    }
  }
#endif // ESP32
}

String IotsaBaseModule::info() {
  // Info method that does nothing, usually overridden for IotsaBaseModule modules
  return "";
}

String IotsaBaseModule::htmlEncode(String data) {
  const char *p = data.c_str();
  String rv = "";
  while(p && *p) {
    char escapeChar = *p++;
    switch(escapeChar) {
      case '&': rv += "&amp;"; break;
      case '<': rv += "&lt;"; break;
      case '>': rv += "&gt;"; break;
      case '"': rv += "&quot;"; break;
      case '\'': rv += "&#x27;"; break;
      case '/': rv += "&#x2F;"; break;
      default: rv += escapeChar; break;
    }
  }
  return rv;
}

//
// Decode percent-escaped string src.
// 
void IotsaBaseModule::percentDecode(const String &src, String &dst) {
    const char *arg = src.c_str();
    dst = String();
    while (*arg) {
      char newch = 0;
      if (*arg == '+') newch = ' ';
      else if (*arg == '%') {
        arg++;
        if (*arg == 0) break;
        if (*arg >= '0' && *arg <= '9') newch = (*arg-'0') << 4;
        if (*arg >= 'a' && *arg <= 'f') newch = (*arg-'a'+10) << 4;
        if (*arg >= 'A' && *arg <= 'F') newch = (*arg-'A'+10) << 4;
        arg++;
        if (*arg == 0) break;
        if (*arg >= '0' && *arg <= '9') newch |= (*arg-'0');
        if (*arg >= 'a' && *arg <= 'f') newch |= (*arg-'a'+10);
        if (*arg >= 'A' && *arg <= 'F') newch |= (*arg-'A'+10);
      } else {
        newch = *arg;
      }
      dst += newch;
      arg++;
    }
}

bool IotsaBaseModule::needsAuthentication(const char *object, IotsaApiOperation verb) {
  bool denied = !app.getAuth()->allows(object, verb);
  if (denied) iotsaApiResult.authResponded = true;
  return denied;
}

bool IotsaBaseModule::needsAuthentication(const char *right) {
  bool denied = !app.getAuth()->allows(right);
  if (denied) iotsaApiResult.authResponded = true;
  return denied;
}

void IotsaBaseModule::lateSetup() {
  // setup method that does nothing, usually overridden for IotsaBaseModule modules
}
