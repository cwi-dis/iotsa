#include "iotsa.h"
#include "iotsaRunmode.h"

// Everything here is a thin call into iotsaController; IotsaRunmodeMod holds no
// state of its own. The persisted settings it touches (configurationModeTimeout,
// wifiDisabledOnBoot, ...) stay owned by IotsaConfigMod / config.cfg -- this
// module only reports the timeout read-only and drives the *runtime* toggles.

void IotsaRunmodeMod::setup() {
}

void IotsaRunmodeMod::lateSetup() {
  api.setup("runmode", true, true);
  name = "runmode";
}

void IotsaRunmodeMod::loop() {
}

#ifdef IOTSA_WITH_WEB
void IotsaRunmodeMod::webHandler() {
  String action;
  if (api.webService->server->hasArg("action")) {
    action = api.webService->server->arg("action");
  }
  String message = "<html><head><title>Iotsa runmode</title></head><body><h1>Iotsa runmode</h1>";

  if (action != "") {
    if (needsAuthentication("config")) return;
    if (action == "setmode") {
      if (api.webService->server->hasArg("mode")) {
        String argValue = api.webService->server->arg("mode");
        if (argValue != "0") {
          iotsaController.requestMode(iotsa_mode(atoi(argValue.c_str())));
        }
      }
      if (api.webService->server->hasArg("factoryreset") && api.webService->server->hasArg("iamsure")
          && api.webService->server->arg("factoryreset") == "1" && api.webService->server->arg("iamsure") == "1") {
        iotsaController.requestMode(IOTSA_MODE_FACTORY_RESET);
      }
      if (iotsaController.requestedMode()) {
        message += "<p><em>Special mode ";
        message += iotsaController.modeName(iotsaController.requestedMode());
        message += " has been requested. Enable within ";
        message += String((iotsaController.requestedModeEndTime() - millis())/1000);
        message += " seconds by power cycling";
        if (iotsaController.rcmInteractionDescription) {
          message += " or ";
          message += iotsaController.rcmInteractionDescription;
        }
        message += ".</em></p>";
      }
    } else if (action == "reboot") {
      iotsaController.requestReboot(2000);
      message += "<p><em>Rebooting in 2 seconds.</em></p>";
    } else if (action == "wifi-disable") {
      iotsaController.setWifiRadioEnabled(false);
      message += "<p><em>WiFi radio disabled.</em></p>";
    } else if (action == "wifi-enable") {
      iotsaController.setWifiRadioEnabled(true);
      message += "<p><em>WiFi radio enabled.</em></p>";
#ifdef IOTSA_WITH_BLE
    } else if (action == "ble-disable") {
      iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_DISABLED;
      iotsaConfig.wantBleModeSwitchAtMillis = millis() + 1000;
      message += "<p><em>BLE radio disabled.</em></p>";
    } else if (action == "ble-enable") {
      iotsaConfig.bleMode = iotsa_ble_mode::IOTSA_BLE_ENABLED;
      iotsaConfig.wantBleModeSwitchAtMillis = millis() + 1000;
      message += "<p><em>BLE radio enabled.</em></p>";
#endif
    }
  }

  // Status.
  if (iotsaController.currentMode()) {
    message += "<p>Currently in mode <b>";
    message += iotsaController.modeName(iotsaController.currentMode());
    message += "</b>, times out in ";
    message += String((iotsaController.currentModeEndTime() - millis())/1000);
    message += " seconds.</p>";
  } else {
    message += "<p>Currently in normal mode.</p>";
  }
  message += "<p>WiFi radio: ";
  message += iotsaStatus.wifiEnabled ? "enabled" : "disabled";
  message += ". Configuration-mode timeout: ";
  message += String(iotsaConfig.configurationModeTimeout);
  message += " seconds.</p>";

  // Request a maintenance mode for the next boot.
  message += "<form method='post'>";
  message += "<input name='mode' type='radio' value='0' checked> Normal mode after next reboot.<br>";
  message += "<input name='mode' type='radio' value='1'> Configuration mode after next reboot.<br>";
  if (iotsaConfig.otaEnabled) {
    message += "<input name='mode' type='radio' value='2'> Over-the-air update mode after next reboot.<br>";
  }
  message += "<br><input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. ";
  message += "<input name='iamsure' type='checkbox' value='1'> Yes, I am sure.<br>";
  message += "<input type='submit' name='action' value='setmode'></form>";

  // Immediate actions.
  message += "<form method='post'>";
  message += "<input type='submit' name='action' value='reboot'> Reboot now.<br>";
  message += "<input type='submit' name='action' value='wifi-disable'> ";
  message += "<input type='submit' name='action' value='wifi-enable'> Disable / enable WiFi radio now.<br>";
#ifdef IOTSA_WITH_BLE
  message += "<input type='submit' name='action' value='ble-disable'> ";
  message += "<input type='submit' name='action' value='ble-enable'> Disable / enable BLE radio now.<br>";
#endif
  message += "</form>";

  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_WEB
String IotsaRunmodeMod::info() {
  String message;
  if (iotsaController.currentMode()) {
    message += "<p>In configuration mode ";
    message += iotsaController.modeName(iotsaController.currentMode());
    message += ", will timeout in " + String((iotsaController.currentModeEndTime()-millis())/1000) + " seconds.</p>";
  } else if (iotsaController.requestedMode()) {
    message += "<p>Special mode ";
    message += iotsaController.modeName(iotsaController.requestedMode());
    message += " has been requested. Enable within ";
    message += String((iotsaController.requestedModeEndTime() - millis())/1000);
    message += " seconds by power cycling";
    if (iotsaController.rcmInteractionDescription) {
      message += " or ";
      message += iotsaController.rcmInteractionDescription;
    }
    message += ".</p>";
  } else if (iotsaController.currentModeEndTime()) {
    message += "<p>Strange, no configuration mode but timeout is " + String(iotsaController.currentModeEndTime()-millis()) + "ms.</p>";
  }
  message += "<p>See <a href=\"/runmode\">/runmode</a> to reboot or change mode.</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

bool IotsaRunmodeMod::getHandler(const char *path, JsonObject& reply) {
  reply["currentMode"] = int(iotsaController.currentMode());
  if (iotsaController.currentMode()) {
    reply["currentModeTimeout"] = (iotsaController.currentModeEndTime() - millis())/1000;
  }
  reply["requestedMode"] = int(iotsaController.requestedMode());
  if (iotsaController.requestedMode()) {
    reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
  }
  reply["modeTimeout"] = iotsaConfig.configurationModeTimeout;   // read-only mirror; owned by /api/config
  reply["wifiDisabled"] = !iotsaStatus.wifiEnabled;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabled"] = iotsaConfig.bleMode == iotsa_ble_mode::IOTSA_BLE_DISABLED;
#endif
  return true;
}

bool IotsaRunmodeMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  JsonObject reqObj = request.as<JsonObject>();

  bool wifiDisabled;
  if (getFromRequest<int>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);
    anyChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<int>(reqObj, "bleDisabled", bleDisabled)) {
    iotsaConfig.bleMode = bleDisabled ? iotsa_ble_mode::IOTSA_BLE_DISABLED : iotsa_ble_mode::IOTSA_BLE_ENABLED;
    iotsaConfig.wantBleModeSwitchAtMillis = millis() + 1000;
    anyChanged = true;
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106).
    iotsaController.requestMode(iotsa_mode(reqModeInt));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      reply["requestedMode"] = int(iotsaController.requestedMode());
      reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
      reply["needsReboot"] = true;
    }
    anyChanged = true;
  }
  if (reqObj["reboot"]) {
    iotsaController.requestReboot(2000);
    anyChanged = true;
  }
  if (checkUnhandled(reqObj)) {
    IotsaSerial.println("Unhandled IotsaApi parameters for /api/runmode");
  }
  return anyChanged;
}
