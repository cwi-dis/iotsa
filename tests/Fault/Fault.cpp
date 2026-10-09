//
// Fault test sketch (cwi-dis/iotsa#285): a minimal iotsa device with one module
// that deliberately does bad things on request, to see how the framework, the
// watchdogs and the other transports react (cwi-dis/iotsa#279, #280).
// Test code only: never include this module in an application.
//
// Everything is triggered with PUT /api/fault (JSON):
//   blockLoop: N        once, busy-wait N ms in this module's next loop()
//   blockLoopEvery: N   busy-wait N ms on every loop() pass (0 stops it)
//   blockHandler: N     busy-wait N ms inside the PUT handler itself, before
//                       replying. Which task that blocks depends on the
//                       transport: the loop() task via REST, and since
//                       cwi-dis/iotsa#236 also via HPS.
//   blockBle: N         busy-wait N ms in the handler of the next write to the
//                       BLE "trigger" characteristic. Since cwi-dis/iotsa#236
//                       BLE handlers run in the loop task, so this blocks
//                       loop(), and the NimBLE host task waits for it (and
//                       gives up after 3 s).
//   replySize: N        GET /api/fault includes a "filler" string of N bytes
//   payload: "..."      any (large) string: the reply reports its length
//   crash: 1            write through a null pointer in the next loop()
//   breadcrumbs: bool   switch breadcrumb writing on/off (iotsaBreadcrumbsEnabled),
//                       to measure its cost with loopsPerSecond
//
// GET /api/fault also reports loopsPerSecond: application.loop() passes counted
// over the last whole second.
//
// Each fault sets an application breadcrumb activity (cwi-dis/iotsa#276) before it
// misbehaves, so after the resulting reset /api/status shows which one it was.
// Every reply includes millisStart/millisEnd (handler entry/exit), and the BLE
// "millis" characteristic returns millis() at the time of its read callback, so
// the timelines of the different transports can be compared. Blocking is a busy
// wait, not delay(): delay() would yield to the scheduler, which is exactly what
// code that blocks for real doesn't do.
//
#include "iotsa.h"
#include "iotsaApi.h"
#ifdef IOTSA_WITH_BLE
#include "iotsaBLEServer.h"
#endif

// Application breadcrumb codes.
enum : uint8_t {
  CRUMB_BLOCK_LOOP = IOTSA_CRUMB_APP,
  CRUMB_BLOCK_HANDLER,
  CRUMB_BLOCK_BLE,
  CRUMB_CRASH
};

static void busyWait(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    // deliberately nothing: no yield(), no delay()
  }
}

class IotsaFaultMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
protected:
  bool getHandler(const char *path, JsonObject& reply) override;
  bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply) override;
  uint32_t _blockLoopOnce = 0;
  uint32_t _blockLoopEvery = 0;
  volatile uint32_t _blockBle = 0;
  uint32_t _replySize = 0;
  bool _crash = false;
  uint32_t _loopCount = 0;
  uint32_t _loopCountStartedAt = 0;
  uint32_t _loopsPerSecond = 0;
#ifdef IOTSA_WITH_BLE
  IotsaBleApiService bleApi;
  bool blePutHandler(UUIDstring charUUID) override;
  bool bleGetHandler(UUIDstring charUUID) override;
  static constexpr UUIDstring serviceUUID = "D9D85F34-4BB7-49C1-8934-56B8994F5411";
  static constexpr UUIDstring triggerUUID = "942E4878-EF95-4B43-BD59-7598F6040A1A";
  static constexpr UUIDstring millisUUID = "EED9BC4C-0357-4E59-9469-D4120C8119DF";
#endif
};

void IotsaFaultMod::setup() {
#ifdef IOTSA_WITH_BLE
  bleApi.setup(serviceUUID, this);
  bleApi.addCharacteristic(triggerUUID, bleApi.BLE_WRITE, NimBLE2904::FORMAT_UINT8, 0x2700, "Write: run the armed blockBle");
  bleApi.addCharacteristic(millisUUID, bleApi.BLE_READ, NimBLE2904::FORMAT_UINT32, 0x2700, "millis() at read");
#endif
}

void IotsaFaultMod::lateSetup() {
  name = "fault";
  api.setup("fault", true, true);
}

void IotsaFaultMod::loop() {
  _loopCount++;
  if (millis() - _loopCountStartedAt >= 1000) {
    _loopsPerSecond = _loopCount;
    _loopCount = 0;
    _loopCountStartedAt = millis();
  }
  if (_crash) {
    iotsaBreadcrumbs.setActivity(CRUMB_CRASH);
    IOTSA_LOG("fault", "crashing now, millis=%lu", (unsigned long)millis());
    volatile int *p = nullptr;
    *p = 42;
  }
  uint32_t ms = _blockLoopOnce ? _blockLoopOnce : _blockLoopEvery;
  if (ms == 0) return;
  _blockLoopOnce = 0;
  iotsaBreadcrumbs.setActivity(CRUMB_BLOCK_LOOP);
  IOTSA_LOG("fault", "loop block %lu ms start, millis=%lu", (unsigned long)ms, (unsigned long)millis());
  busyWait(ms);
  IOTSA_LOG("fault", "loop block end, millis=%lu", (unsigned long)millis());
}

bool IotsaFaultMod::getHandler(const char *path, JsonObject& reply) {
  reply["millisStart"] = millis();
  reply["blockLoopEvery"] = _blockLoopEvery;
  reply["blockBle"] = (uint32_t)_blockBle;
  reply["replySize"] = _replySize;
  reply["breadcrumbs"] = iotsaBreadcrumbsEnabled;
  reply["loopsPerSecond"] = _loopsPerSecond;
  if (_replySize) {
    String filler;
    filler.reserve(_replySize);
    for (uint32_t i = 0; i < _replySize; i++) filler += 'x';
    reply["filler"] = filler;
  }
  reply["millisEnd"] = millis();
  return true;
}

bool IotsaFaultMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  reply["millisStart"] = millis();
  JsonObject reqObj = request.as<JsonObject>();
  bool any = false;
  uint32_t blockHandler = 0;
  if (getFromRequest<int>(reqObj, "blockLoop", _blockLoopOnce)) any = true;
  if (getFromRequest<int>(reqObj, "blockLoopEvery", _blockLoopEvery)) any = true;
  uint32_t ble;
  if (getFromRequest<int>(reqObj, "blockBle", ble)) { _blockBle = ble; any = true; }
  if (getFromRequest<int>(reqObj, "replySize", _replySize)) any = true;
  if (getFromRequest<bool>(reqObj, "crash", _crash)) any = true;
  if (getFromRequest<bool>(reqObj, "breadcrumbs", iotsaBreadcrumbsEnabled)) any = true;
  const char *payload = nullptr;
  if (getFromRequest<const char *>(reqObj, "payload", payload)) {
    reply["payloadLength"] = payload ? strlen(payload) : 0;
    any = true;
  }
  if (getFromRequest<int>(reqObj, "blockHandler", blockHandler)) {
    IotsaActivityScope activity(CRUMB_BLOCK_HANDLER);
    IOTSA_LOG("fault", "handler block %lu ms start, millis=%lu", (unsigned long)blockHandler, (unsigned long)millis());
    busyWait(blockHandler);
    IOTSA_LOG("fault", "handler block end, millis=%lu", (unsigned long)millis());
    any = true;
  }
  checkUnhandled(reqObj);
  reply["millisEnd"] = millis();
  return any;
}

#ifdef IOTSA_WITH_BLE
bool IotsaFaultMod::blePutHandler(UUIDstring charUUID) {
  if (charUUID == triggerUUID) {
    uint32_t ms = _blockBle;
    _blockBle = 0;
    IotsaActivityScope activity(CRUMB_BLOCK_BLE);
    IOTSA_LOG("fault", "BLE block %lu ms start, millis=%lu", (unsigned long)ms, (unsigned long)millis());
    busyWait(ms);
    IOTSA_LOG("fault", "BLE block end, millis=%lu", (unsigned long)millis());
    return true;
  }
  return false;
}

bool IotsaFaultMod::bleGetHandler(UUIDstring charUUID) {
  if (charUUID == millisUUID) {
    bleApi.set(millisUUID, (uint32_t)millis());
    return true;
  }
  return false;
}
#endif

#ifdef IOTSA_WITH_WEB
String IotsaFaultMod::info() {
  return "<p>Fault test module: PUT /api/fault to make this device misbehave (see tests/Fault).</p>";
}
#endif

IotsaApplication application("Iotsa Fault test");
IotsaFaultMod faultMod(application);

void setup(void) {
  application.setup();
  application.lateSetup();
}

void loop(void) {
  application.loop();
}
