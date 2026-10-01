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
  pendingDeadlineAtMillis = millis() + commandTimeoutMillis;
  lastCommandStatus = "pending...";
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

void IotsaRunmodeBLEClient::serviceIfNeeded() {
  if (pendingCommand == PendingCommand::None) return;
  if (millis() > pendingDeadlineAtMillis) {
    lastCommandStatus = "gave up: device not reachable";
    pendingCommand = PendingCommand::None;
    return;
  }
  if (!available()) return; // still waiting for a scan to find it
  if (!isConnected()) {
    if (isDisconnecting()) return; // previous disconnect still settling
    if (!canConnect()) return; // radio busy (scanning or another connect), try again next tick
    if (!connect()) return; // failed this attempt, retry until the deadline
  }
  bool ok = false;
  switch (pendingCommand) {
    case PendingCommand::Identify: ok = identify(); break;
    case PendingCommand::Reboot: ok = reboot(); break;
    case PendingCommand::PromoteMode: ok = promoteMode(); break;
    case PendingCommand::SetWifiDisabled: ok = setWifiDisabled(pendingSetWifiDisabledValue); break;
    default: break;
  }
  lastCommandStatus = ok ? "done" : "command failed";
  pendingCommand = PendingCommand::None;
  disconnect();
}

void IotsaRunmodeBLEClient::getHandler(JsonObject& reply) {
  IotsaBLEClientDevice::getHandler(reply);
  reply["commandPending"] = pendingCommand != PendingCommand::None;
  if (lastCommandStatus != "") reply["lastCommandStatus"] = lastCommandStatus;
}

bool IotsaRunmodeBLEClient::putHandler(const JsonVariant& request) {
  bool any = IotsaBLEClientDevice::putHandler(request);
  if (!request.is<JsonObject>()) return any;
  const JsonObject& reqObj = request.as<JsonObject>();
  if (reqObj["identify"] | 0) any |= queueIdentify();
  if (reqObj["reboot"] | 0) any |= queueReboot();
  if (reqObj["promoteMode"] | 0) any |= queuePromoteMode();
  bool wifiDisabled;
  if (getFromRequest<bool>(reqObj, "setWifiDisabled", wifiDisabled)) any |= queueSetWifiDisabled(wifiDisabled);
  return any;
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
  if (lastCommandStatus != "") {
    message += "<p>Last command: " + lastCommandStatus + "</p>";
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
