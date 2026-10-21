#include "iotsaBLEButtonMod.h"
#include "iotsaConfigFile.h"

// Implementation of the BLE doorbell button module. It is built like the
// HelloIotsa module, but what it configures is not a name but another device:
// ringer, which knows how to find a BLERinger over BLE and ring it.

void IotsaBLEButtonMod::setup() {
  configLoad();
  // The button calls us when it is pressed.
  button.setCallback([this]() { ring(); return true; });
}

void IotsaBLEButtonMod::lateSetup() {
  api.setup("doorbell", true, true);
  name = "doorbell";
#ifdef IOTSA_WITH_WEB
  addTrigger("ring", [this](IotsaWebContext& ctx) { ringTrigger(ctx); });
#endif
}

// loop() is empty (in the .h): the button calls ring(), and ringer does its
// work in the BLE client module's loop().

// The target is saved under "target": its name, and where it was found.
void IotsaBLEButtonMod::configLoad() {
  IotsaConfigFileLoad cf("/config/doorbell.cfg");
  ringer.configLoad(cf, "target");
}

void IotsaBLEButtonMod::configSave() {
  IotsaConfigFileSave cf("/config/doorbell.cfg");
  ringer.configSave(cf, "target");
}

// Ringing over BLE takes a while (find the ringer, connect, ring,
// disconnect), so this only asks for it. ringStatus() tells how it went.
void IotsaBLEButtonMod::ring() {
  if (ringer.getName().empty()) {
    noTargetConfigured = true;
    return;
  }
  noTargetConfigured = false;
  ringer.requestRing(ringTimeoutMillis);
}

String IotsaBLEButtonMod::ringStatus() {
  if (noTargetConfigured) return "no target configured";
  if (ringer.hasPendingWork()) return "ringing...";
  const char *status = ringer.getLastWorkStatus();
  return status ? status : "";
}

// The target's fields are ringer's own, so it handles them.
bool IotsaBLEButtonMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  bool ringerChanged = false;
  if (!ringer.putHandler(request, reply, &ringerChanged)) return false;
  if (ringerChanged) {
    configSave();
    if (changed) *changed = true;
  }
  return true;
}

bool IotsaBLEButtonMod::getHandler(const char *path, JsonObject& reply) {
  ringer.getHandler(reply);
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaBLEButtonMod::formHandler_args(IotsaWebContext& ctx) {
  {
    IotsaWebContext::Scope s(ctx, "target");
    ringer.formHandler_args(ctx);
  }
  if (ctx.argAction("ringnow", IotsaWebContext::Transient)) {
    ring();
    ctx.feedback("Ringing.");
  }
}

void IotsaBLEButtonMod::formHandler_fields(IotsaWebContext& ctx) {
  {
    IotsaWebContext::Scope s(ctx, "target");
    ringer.formHandler_fields(ctx);
  }
  if (!ctx.includesEveryday()) return;
  // How the last ring went shows up on the next page load.
  String status = ringStatus();
  if (status != "") ctx.showStatus("Last ring attempt", "", status);
  if (ringer.available()) ctx.showButton("ringnow", "Ring now", IotsaWebContext::Transient);
}

// The trigger, /ring: rings the target, like pressing the button.
void IotsaBLEButtonMod::ringTrigger(IotsaWebContext& ctx) {
  ring();
  if (noTargetConfigured) {
    ctx.error("No target configured.");
  } else {
    ctx.feedback("Ringing.");
  }
}
#endif // IOTSA_WITH_WEB

String IotsaBLEButtonMod::info() {
  return "<p>Rings a BLERinger over BLE when the button is pressed. See <a href='/doorbell'>/doorbell</a> to set the target, or call <a href='/ring'>/ring</a>.</p>";
}
