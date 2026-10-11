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

bool IotsaRunmodeBLEClient::putHandler(const JsonVariant& request, JsonObject& reply) {
  if (!IotsaBLEClientDevice::putHandler(request, reply)) return false;
  const JsonObject& reqObj = request.as<JsonObject>();
  // One command at a time: queueCommand() refuses while one is pending.
  bool ok = true;
  bool flag;
  if (getFromRequest<bool>(reqObj, "identify", flag) && flag) ok &= queueIdentify();
  if (getFromRequest<bool>(reqObj, "reboot", flag) && flag) ok &= queueReboot();
  if (getFromRequest<bool>(reqObj, "promoteMode", flag) && flag) ok &= queuePromoteMode();
  bool wifiDisabled;
  if (getFromRequest<bool>(reqObj, "setWifiDisabled", wifiDisabled)) ok &= queueSetWifiDisabled(wifiDisabled);
  if (!ok) return apiError(409, "busy: a command for this device is still pending");
  return true;
}

#ifdef IOTSA_WITH_WEB
void IotsaRunmodeBLEClient::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) {
  IotsaBLEClientDevice::formHandler_fields(message, text, f_name, includeConfig);
  if (includeConfig) {
    message += "<form method='get'><input type='hidden' name='" + f_name + ".identify' value='1'><input type='submit' value='Identify'></form>";
    message += "<form method='get'><input type='hidden' name='" + f_name + ".reboot' value='1'><input type='submit' value='Reboot'></form>";
    message += "<form method='get'><input type='hidden' name='" + f_name + ".promoteMode' value='1'><input type='submit' value='Promote pending mode'></form>";
    message += "<form method='get'><input type='hidden' name='" + f_name + ".setWifiDisabled' value='0'><input type='submit' value='Enable WiFi'></form>";
    message += "<form method='get'><input type='hidden' name='" + f_name + ".setWifiDisabled' value='1'><input type='submit' value='Disable WiFi'></form>";
  }
  if (workPending) {
    message += "<p>Command pending...</p>";
  } else if (lastWorkStatus) {
    message += "<p>Last command: " + String(lastWorkStatus) + "</p>";
  }
}

bool IotsaRunmodeBLEClient::formHandler_args(IotsaWebServer *server, const String& f_name, bool includeConfig) {
  bool any = IotsaBLEClientDevice::formHandler_args(server, f_name, includeConfig);
  if (!includeConfig) return any;
  if (server->hasArg(f_name + ".identify")) any |= queueIdentify();
  if (server->hasArg(f_name + ".reboot")) any |= queueReboot();
  if (server->hasArg(f_name + ".promoteMode")) any |= queuePromoteMode();
  if (server->hasArg(f_name + ".setWifiDisabled")) {
    bool disabled = server->arg(f_name + ".setWifiDisabled").toInt() != 0;
    any |= queueSetWifiDisabled(disabled);
  }
  return any;
}
#endif // IOTSA_WITH_WEB
#endif // IOTSA_WITH_BLE
