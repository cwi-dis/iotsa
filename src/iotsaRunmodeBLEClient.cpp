#include "iotsaRunmodeBLEClient.h"
#ifdef IOTSA_WITH_BLE

bool IotsaRunmodeBLEClient::getCurrentMode(uint8_t& mode) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::currentModeUUID);
  return get(svc, chr, mode);
}

bool IotsaRunmodeBLEClient::requestMode(uint8_t mode) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::requestedModeUUID);
  return set(svc, chr, mode);
}

bool IotsaRunmodeBLEClient::reboot() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::rebootUUID);
  return set(svc, chr, (uint8_t)1);
}

bool IotsaRunmodeBLEClient::promoteMode() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::promoteModeUUID);
  return set(svc, chr, (uint8_t)1);
}

bool IotsaRunmodeBLEClient::setWifiDisabled(bool disabled) {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::wifiDisabledUUID);
  return set(svc, chr, (uint8_t)(disabled ? 1 : 0));
}

bool IotsaRunmodeBLEClient::identify() {
  NimBLEUUID svc(IotsaRunmodeBLE::serviceUUID);
  NimBLEUUID chr(IotsaRunmodeBLE::identifyUUID);
  return set(svc, chr, (uint8_t)1);
}

bool IotsaRunmodeBLEClient::queueCommand(PendingCommand cmd) {
  if (pendingCommand != PendingCommand::None) return false;
  pendingCommand = cmd;
  requestWork(commandTimeoutMillis);
  return true;
}

bool IotsaRunmodeBLEClient::queueIdentify() { return queueCommand(PendingCommand::Identify); }
bool IotsaRunmodeBLEClient::queueReboot() { return queueCommand(PendingCommand::Reboot); }
bool IotsaRunmodeBLEClient::queuePromoteMode() { return queueCommand(PendingCommand::PromoteMode); }
bool IotsaRunmodeBLEClient::queueSetWifiDisabled(bool disabled) {
  if (!queueCommand(PendingCommand::SetWifiDisabled)) return false;
  pendingSetWifiDisabledValue = disabled;
  return true;
}

bool IotsaRunmodeBLEClient::doWork() {
  bool ok = true;
  switch (pendingCommand) {
    case PendingCommand::Identify: ok = identify(); break;
    case PendingCommand::Reboot: ok = reboot(); break;
    case PendingCommand::PromoteMode: ok = promoteMode(); break;
    case PendingCommand::SetWifiDisabled: ok = setWifiDisabled(pendingSetWifiDisabledValue); break;
    default: break;
  }
  pendingCommand = PendingCommand::None;
  return ok;
}

void IotsaRunmodeBLEClient::workAbandoned() {
  pendingCommand = PendingCommand::None;
}

void IotsaRunmodeBLEClient::putHandler(IotsaApiContext& ctx) {
  IotsaBLEClientDevice::putHandler(ctx);
  bool identify = false;
  ctx.arg("identify", identify, IotsaApiContext::Transient);
  bool reboot = false;
  ctx.arg("reboot", reboot, IotsaApiContext::Transient);
  bool promoteMode = false;
  ctx.arg("promoteMode", promoteMode, IotsaApiContext::Transient);
  // Both true and false are a command.
  bool setWifi = ctx.argAction("setWifiDisabled", IotsaApiContext::Transient);
  if (ctx.failed()) return;
  // One command at a time: queueCommand() refuses while one is pending.
  bool ok = true;
  if (identify) ok &= queueIdentify();
  if (reboot) ok &= queueReboot();
  if (promoteMode) ok &= queuePromoteMode();
  if (setWifi) ok &= queueSetWifiDisabled(ctx.request()["setWifiDisabled"].as<bool>());
  if (!ok) ctx.error(409, "busy: a command for this device is still pending");
}

#ifdef IOTSA_WITH_WEB
// Commands for the remote device: they happen now, once it can be reached.
// Only one at a time.
static void _queued(IotsaWebContext& ctx, bool ok) {
  if (ok) {
    ctx.feedback("Command queued, sent when the device can be reached.");
  } else {
    ctx.error("Busy: a command for this device is still pending.");
  }
}

void IotsaRunmodeBLEClient::formHandler_args(IotsaWebContext& ctx) {
  IotsaBLEClientDevice::formHandler_args(ctx);
  if (ctx.argAction("identify", IotsaWebContext::Transient)) _queued(ctx, queueIdentify());
  if (ctx.argAction("reboot", IotsaWebContext::Transient)) _queued(ctx, queueReboot());
  if (ctx.argAction("promoteMode", IotsaWebContext::Transient)) _queued(ctx, queuePromoteMode());
  if (ctx.argAction("enableWifi", IotsaWebContext::Transient)) _queued(ctx, queueSetWifiDisabled(false));
  if (ctx.argAction("disableWifi", IotsaWebContext::Transient)) _queued(ctx, queueSetWifiDisabled(true));
}

void IotsaRunmodeBLEClient::formHandler_fields(IotsaWebContext& ctx) {
  IotsaBLEClientDevice::formHandler_fields(ctx);
  if (!ctx.includesEveryday()) return;
  if (workPending) {
    ctx.showStatus("Command", "", "pending");
  } else if (lastWorkStatus) {
    ctx.showStatus("Last command", "", lastWorkStatus);
  }
  ctx.showButton("identify", "Identify", IotsaWebContext::Transient);
  ctx.showButton("reboot", "Reboot", IotsaWebContext::Transient);
  ctx.showButton("promoteMode", "Confirm requested mode", IotsaWebContext::Transient);
  ctx.showButton("enableWifi", "Enable WiFi", IotsaWebContext::Transient);
  ctx.showButton("disableWifi", "Disable WiFi", IotsaWebContext::Transient);
}
#endif // IOTSA_WITH_WEB
#endif // IOTSA_WITH_BLE
