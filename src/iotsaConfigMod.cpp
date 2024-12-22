#include <Esp.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <esp_log.h>
#include <rom/rtc.h>
#else
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif

#include "iotsa.h"
#include "iotsaConfigFile.h"
#include "iotsaConfigMod.h"
#include "iotsaFS.h"

#ifdef IOTSA_WITH_HTTPS
#include <libb64/cdecode.h>
#include <libb64/cencode.h>
#endif // IOTSA_WITH_HTTPS

void IotsaConfigMod::setup() {
  IFDEBUG IotsaSerial.print("boot reason: ");
  IFDEBUG IotsaSerial.println(iotsaConfig.getBootReason());
  iotsaConfig.setDefaultCertificate();
  configLoad();
  if (app.status) app.status->showStatus();
  if (iotsaConfig.configurationMode) {
  	IFDEBUG IotsaSerial.println("configurationMode, re-saving config.cfg without it");
  	configSave();
    iotsaConfig.configurationModeEndTime = millis() + 1000*iotsaConfig.configurationModeTimeout;
    IFDEBUG IotsaSerial.print("configurationMode=");
    IFDEBUG IotsaSerial.print((int)iotsaConfig.configurationMode);
    IFDEBUG IotsaSerial.print(", timeout at ");
    IFDEBUG IotsaSerial.println(iotsaConfig.configurationModeEndTime);
}
  // If a configuration mode was requested but the reset reason was not
  // external reset (the button) or powerup we do not honor the configuration mode
  // request: it could be triggered through a software bug or so, and we want to require
  // user interaction.
#ifndef ESP32
  rst_info *rip = ESP.getResetInfoPtr();
  int reason = (int)rip->reason;
  bool badReason = rip->reason != REASON_DEFAULT_RST && rip->reason != REASON_EXT_SYS_RST;
#else
  int reason = rtc_get_reset_reason(0);
  // xxxjack Not sure why I sometimes see the WDT reset on pressing the reset button...
  bool badReason = reason != POWERON_RESET && reason != RTCWDT_RTC_RESET;
#endif
  if (badReason && iotsaConfig.configurationMode != IOTSA_MODE_NORMAL) {
    iotsaConfig.configurationMode = IOTSA_MODE_NORMAL;
    iotsaConfig.configurationModeEndTime = 0;
    IFDEBUG IotsaSerial.print("configurationMode not honoured because of reset reason:");
    IFDEBUG IotsaSerial.println(reason);
  }
  // If factory reset is requested format the Flash and reboot
  if (iotsaConfig.configurationMode == IOTSA_MODE_FACTORY_RESET) {
    iotsaConfig.factoryReset();
  }
  if (app.status) app.status->showStatus();
}

#ifdef IOTSA_WITH_WEB
void
IotsaConfigMod::handler() {
  bool wrongMode = false;
  bool anyChanged = false;
  bool hostnameChanged = false;
  if( server->hasArg("hostName")) {
    String argValue = server->arg("hostName");
    if (argValue != iotsaConfig.hostName) {
      if (iotsaConfig.inConfigurationOrFactoryMode()) {
        if (needsAuthentication("config")) return;
        iotsaConfig.hostName = argValue;
        anyChanged = true;
        hostnameChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  if( server->hasArg("rebootTimeout")) {
    int newValue = server->arg("rebootTimeout").toInt();
    if (newValue != iotsaConfig.configurationModeTimeout) {
      if (iotsaConfig.inConfigurationMode(true)) {
        if (needsAuthentication("config")) return;
        iotsaConfig.configurationModeTimeout = newValue;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  if( server->hasArg("mode")) {
    String argValue = server->arg("mode");
    if (argValue != "0") {
      if (needsAuthentication("config")) return;
      iotsaConfig.nextConfigurationMode = config_mode(atoi(argValue.c_str()));
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      anyChanged = true;
    }
  }
#ifdef IOTSA_WITH_HTTPS
  if (server->hasArg("httpsKey") && server->arg("httpsKey") != "") {
    if (iotsaConfig.inConfigurationMode(true)) {
      if (needsAuthentication("config")) return;
      String b64String(server->arg("httpsKey"));
      if (b64String.startsWith("-----BEGIN RSA PRIVATE KEY-----")) {
        // Strip DER header and footer
        int first = b64String.indexOf('\n');
        int last = b64String.lastIndexOf("-----END RSA PRIVATE KEY-----");
        if (first >= 0 && last >= 0) {
          b64String = b64String.substring(first, last);
        } else {
          IFDEBUG IotsaSerial.println("httpsKey bad format PEM");
          b64String = "";
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsKey bad PEM header");
        b64String = "";
      }
      const char *b64Value = b64String.c_str();
      int b64len = strlen(b64Value);
      int expDecodeLen = base64_decode_expected_len(b64len);
      char *tmpValue = (char *)malloc(expDecodeLen);
      if (tmpValue) {
        int decLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decLen > 0) {
          newKey = (uint8_t *)tmpValue;
          newKeyLength = decLen;
          IFDEBUG IotsaSerial.print("Decoded httpsKey len=");
          IFDEBUG IotsaSerial.print(decLen);
          IFDEBUG IotsaSerial.print(" expLen=");
          IFDEBUG IotsaSerial.print(expDecodeLen);
          IFDEBUG IotsaSerial.print(" b64len=");
          IFDEBUG IotsaSerial.println(b64len);
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("Error base64 decoding httpsKey");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsKey malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
  if (server->hasArg("httpsCertificate") && server->arg("httpsCertificate") != "") {
    if (iotsaConfig.inConfigurationMode(true)) {
      if (needsAuthentication("config")) return;
      String b64String(server->arg("httpsCertificate"));
      if (b64String.startsWith("-----BEGIN CERTIFICATE-----")) {
        // Strip DER header and footer
        int first = b64String.indexOf('\n');
        int last = b64String.lastIndexOf("-----END CERTIFICATE-----");
        if (first >= 0 && last >= 0) {
          b64String = b64String.substring(first, last);
        } else {
          IFDEBUG IotsaSerial.println("httpsCertificate bad format PEM");
          b64String = "";
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsCertificate bad PEM header");
        b64String = "";
      }
      const char *b64Value = b64String.c_str();
      int b64len = strlen(b64Value);
      int expDecodeLen = base64_decode_expected_len(b64len);
      IFDEBUG IotsaSerial.print("httpsCertificate expected len=");
      IFDEBUG IotsaSerial.println(expDecodeLen);
      char *tmpValue = (char *)malloc(expDecodeLen);
      if (tmpValue) {
        int decLen = base64_decode_chars(b64Value, b64len, tmpValue);
        if (decLen > 0) {
          newCertificate = (uint8_t *)tmpValue;
          newCertificateLength = decLen;
          IFDEBUG IotsaSerial.print("Decoded httpsCertificate len=");
          IFDEBUG IotsaSerial.print(decLen);
          IFDEBUG IotsaSerial.print(" expLen=");
          IFDEBUG IotsaSerial.print(expDecodeLen);
          IFDEBUG IotsaSerial.print(" b64len=");
          IFDEBUG IotsaSerial.println(b64len);
          anyChanged = true;
        } else {
          IFDEBUG IotsaSerial.println("Error base64 decoding httpsCertificate");
        }
      } else {
        IFDEBUG IotsaSerial.println("httpsCertificate malloc failed");
      }
    } else {
      wrongMode = true;
    }
  }
#endif // IOTSA_WITH_HTTPS
  if( server->hasArg("factoryreset") && server->hasArg("iamsure")) {
    if (server->arg("factoryreset") == "1" && server->arg("iamsure") == "1") {
      iotsaConfig.nextConfigurationMode = IOTSA_MODE_FACTORY_RESET;
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      anyChanged = true;
    }
  }
 if( server->hasArg("wifiDisabledOnBoot")) {
    int newValue = server->arg("wifiDisabledOnBoot").toInt();
    if ((bool)newValue != iotsaConfig.wifiDisabledOnBoot) {
      if (iotsaConfig.inConfigurationMode(true)) {
        if (needsAuthentication("config")) return;
        iotsaConfig.wifiDisabledOnBoot = (bool)newValue;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
#ifdef IOTSA_WITH_BLE
 if( server->hasArg("bleDisabledOnBoot")) {
    int newValue = server->arg("bleDisabledOnBoot").toInt();
    if ((bool)newValue != iotsaConfig.bleDisabledOnBoot) {
      if (iotsaConfig.inConfigurationMode(true)) {
        if (needsAuthentication("config")) return;
        iotsaConfig.bleDisabledOnBoot = (bool)newValue;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
#endif
  String message = "<html><head><title>Iotsa configuration</title></head><body><h1>Iotsa configuration</h1>";
  if (wrongMode) {
    message += "<p><em>Error:</em> must be in configuration mode to change some of these parameters.</p>";
  }
  if (anyChanged) {
    configSave();
    message += "<p>Settings saved to Flash.</p>";
    if (hostnameChanged) {
      message += "<p><em>Rebooting device to change hostname</em>.</p>";
    }
    if (iotsaConfig.nextConfigurationMode) {
      message += "<p><em>Special mode ";
      message += iotsaConfig.modeName(iotsaConfig.nextConfigurationMode);
      message += " has been requested. Enable within ";
      message += String((iotsaConfig.nextConfigurationModeEndTime - millis())/1000);
      message += " seconds by power cycling";
      if (iotsaConfig.rcmInteractionDescription) {
        message += " or ";
        message += iotsaConfig.rcmInteractionDescription;
      }
      message += ".</em></p>";
    }
  }
  if (!iotsaConfig.inConfigurationMode()) {
    message += "<p>Hostname: ";
    message += htmlEncode(iotsaConfig.hostName);
    message += " (goto configuration mode to change)<br>Configuration mode timeout: ";
    message += String(iotsaConfig.configurationModeTimeout);
    message += " (goto configuration mode to change)";
    if (iotsaConfig.wifiDisabledOnBoot) {
      message += "Wifi disabled on boot.<br>";
    }
#ifdef IOTSA_WITH_BLE
    if (iotsaConfig.bleDisabledOnBoot) {
      message += "BLE disabled on boot.<br>";
    }
#endif
    message += "</p>";
#ifdef IOTSA_WITH_HTTPS
    if (iotsaConfig.usingDefaultCertificate()) {
      message += "<p>Using factory-installed (<b>not very secure</b>) https certificate</p>";
    } else {
      message += "<p>Using uploaded https certificate.</p>";
    }
#endif // IOTSA_WITH_HTTPS
  }
  message += "<form method='post'>";
  if (iotsaConfig.inConfigurationOrFactoryMode()) {
    message += "Hostname: <input name='hostName' value='";
    message += htmlEncode(iotsaConfig.hostName);
    message += "'><br>";
  }
  if (iotsaConfig.inConfigurationMode()) {
    message += "Configuration mode timeout: <input name='rebootTimeout' value='";
    message += String(iotsaConfig.configurationModeTimeout);
    message += "'><br>";
#ifdef IOTSA_WITH_HTTPS
    message += "HTTPS private key (PEM): <br><textarea name='httpsKey' rows='8' cols='60'></textarea><br>";
    message += "HTTPS certificate (PEM): <br><textarea name='httpsCertificate' rows='8' cols='60'></textarea><br>";
#endif
  }
  message += "<input name='wifiDisabledOnBoot' type='checkbox' value='1'";
  if (iotsaConfig.wifiDisabledOnBoot) message += " checked";
  message += "> Wifi disabled on boot.<br>";
#ifdef IOTSA_WITH_BLE
  message += "<input name='bleDisabledOnBoot' type='checkbox' value='1'";
  if (iotsaConfig.bleDisabledOnBoot) message += " checked";
  message += "> BLE disabled on boot.<br>";
#endif
  message += "<input name='mode' type='radio' value='0' checked> Enter normal mode after next reboot.<br>";
  message += "<input name='mode' type='radio' value='1'> Enter configuration mode after next reboot.<br>";
  if (iotsaConfig.otaEnabled) {
    message += "<input name='mode' type='radio' value='2'> Enable over-the-air update after next reboot.<br>";
  }
  message += "<br><input name='factoryreset' type='checkbox' value='1'> Factory-reset and clear all files. <input name='iamsure' type='checkbox' value='1'> Yes, I am sure.<br>";
  message += "<input type='submit'></form>";
  message += "</body></html>";
  server->send(200, "text/html", message);
  if (hostnameChanged) {
    iotsaConfig.requestReboot(2000);
  }
}

String IotsaConfigMod::info() {
  String message;
  if (iotsaConfig.configurationMode) {
  	message += "<p>In configuration mode ";
    message += iotsaConfig.modeName(iotsaConfig.configurationMode);
    message += ", will timeout in " + String((iotsaConfig.configurationModeEndTime-millis())/1000) + " seconds.</p>";
  } else if (iotsaConfig.nextConfigurationMode) {
    message += "<p>Special mode ";
    message += iotsaConfig.modeName(iotsaConfig.nextConfigurationMode);
    message += " has been requested. Enable within ";
    message += String((iotsaConfig.nextConfigurationModeEndTime - millis())/1000);
    message += " seconds by power cycling";
    if (iotsaConfig.rcmInteractionDescription) {
      message += " or ";
      message += iotsaConfig.rcmInteractionDescription;
    }
    message += ".</p>";
  } else if (iotsaConfig.configurationModeEndTime) {
  	message += "<p>Strange, no configuration mode but timeout is " + String(iotsaConfig.configurationModeEndTime-millis()) + "ms.</p>";
  }
  message += "<p>" + app.title + " is based on iotsa " + IOTSA_FULL_VERSION + ". See <a href=\"/config\">/config</a> to change configuration.<br>";
  message += "Last boot " + String((int)millis()/1000) + " seconds ago, reason ";
  message += iotsaConfig.getBootReason();
  message += ".</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

#ifdef IOTSA_WITH_API
bool IotsaConfigMod::getHandler(const char *path, JsonObject& reply) {
  if (strcmp(path, "/api/version") == 0) {
    reply["iotsaVersion"] = IOTSA_VERSION;
    reply["iotsaFullVersion"] = IOTSA_FULL_VERSION;
#ifdef IOTSA_CONFIG_PROGRAM_NAME
    reply["programName"] = IOTSA_CONFIG_PROGRAM_NAME;
#endif
#ifdef IOTSA_CONFIG_PROGRAM_VERSION
    reply["programVersion"] = IOTSA_CONFIG_PROGRAM_VERSION;
#endif
#ifdef IOTSA_CONFIG_PROGRAM_REPO
    reply["programRepo"] = IOTSA_CONFIG_PROGRAM_REPO;
#endif
#ifdef ARDUINO_VARIANT
    reply["board"] = ARDUINO_VARIANT;
#elif defined(ARDUINO_BOARD)
    reply["board"] = ARDUINO_BOARD;
#endif
    return true;
  }
  reply["hostName"] = iotsaConfig.hostName;
  reply["modeTimeout"] = iotsaConfig.configurationModeTimeout;
  reply["currentMode"] = int(iotsaConfig.configurationMode);
  if (iotsaConfig.configurationMode) {
    reply["currentModeTimeout"] = (iotsaConfig.configurationModeEndTime - millis())/1000;
  }
  reply["privateWifi"] = iotsaConfig.wifiMode == IOTSA_WIFI_FACTORY || iotsaConfig.wifiMode == IOTSA_WIFI_NOTFOUND;
  reply["mdnsEnabled"] = iotsaConfig.mdnsEnabled;
  reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
  if (iotsaConfig.nextConfigurationMode) {
    reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
  }
  reply["wifiDisabled"] = iotsaConfig.wifiMode == iotsa_wifi_mode::IOTSA_WIFI_DISABLED;
  reply["wifiDisabledOnBoot"] = iotsaConfig.wifiDisabledOnBoot;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabled"] = iotsaConfig.bleMode == iotsa_ble_mode::IOTSA_BLE_DISABLED;
  reply["bleDisabledOnBoot"] = iotsaConfig.bleDisabledOnBoot;
#endif
  reply["program"] = app.title;
#ifdef IOTSA_WITH_HTTPS
  reply["defaultCert"] = iotsaConfig.usingDefaultCertificate();
  reply["has_httpsKey"] = IOTSA_FS.exists("/config/httpsKey.der");
  reply["has_httpsCert"] = IOTSA_FS.exists("/config/httpsCert.der");
#endif
  reply["bootCause"] = iotsaConfig.getBootReason();
  reply["uptime"] = millis() / 1000;
  JsonArray modules = reply["modules"].to<JsonArray>();
  modules.add("version");
  for (IotsaBaseMod *m=app.firstEarlyModule; m; m=m->nextModule) {
    if (m->name != "")
      modules.add(m->name);
  }
  for (IotsaBaseMod *m=app.firstModule; m; m=m->nextModule) {
    if (m->name != "")
      modules.add(m->name);
  }
  JsonArray features = reply["features"].to<JsonArray>();
#ifdef IOTSA_WITH_HTTP
  features.add("http");
#endif
#ifdef IOTSA_WITH_HTTPS
  features.add("https");
#endif
#ifdef IOTSA_WITH_COAP
  features.add("coap");
#endif
#ifdef IOTSA_WITH_HPS
  features.add("hps");
#endif
#ifdef IOTSA_WITH_OTA
  features.add("ota");
#endif
#ifdef IOTSA_WITH_BLE
  features.add("ble");
#endif
  if (iotsaConfig.mdnsEnabled) features.add("mdns");

  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool radioModeChanged = false;

  JsonObject reqObj = request.as<JsonObject>();
  // First look for arguments that are also valid in normal mode.
  bool wifiDisabled;
  if (getFromRequest<bool>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsa_wifi_mode newMode = wifiDisabled ? iotsa_wifi_mode::IOTSA_WIFI_DISABLED : iotsa_wifi_mode::IOTSA_WIFI_NORMAL;
    iotsaConfig.wifiMode = newMode;
    iotsaConfig.wantWifiModeSwitchAtMillis = millis()+1000;
    radioModeChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<bool>(reqObj, "bleDisabled", bleDisabled)) {
    iotsa_ble_mode newMode = bleDisabled ? iotsa_ble_mode::IOTSA_BLE_DISABLED : iotsa_ble_mode::IOTSA_BLE_ENABLED;
    iotsaConfig.bleMode = newMode;
    iotsaConfig.wantBleModeSwitchAtMillis = millis()+1000;
    radioModeChanged = true;
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    iotsaConfig.nextConfigurationMode = config_mode(reqModeInt);
    anyChanged = iotsaConfig.nextConfigurationMode != config_mode(0);
    if (anyChanged) {
      iotsaConfig.nextConfigurationModeEndTime = millis() + iotsaConfig.configurationModeTimeout*1000;
      reply["requestedMode"] = int(iotsaConfig.nextConfigurationMode);
      reply["requestedModeTimeout"] = (iotsaConfig.nextConfigurationModeEndTime - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  if (!iotsaConfig.inConfigurationOrFactoryMode()) {
    IFDEBUG IotsaSerial.println("Not in config mode");
    if (reqObj["reboot"]) {
      iotsaConfig.requestReboot(2000);
      anyChanged = true;
    }
    return anyChanged||radioModeChanged;
  }
  if (getFromRequest<const char *>(reqObj, "hostName", iotsaConfig.hostName)) {
    anyChanged = true;
    reply["needsReboot"] = true;
  }
  if (getFromRequest<bool>(reqObj, "wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot)) {
    anyChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  if (getFromRequest<bool>(reqObj, "bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot)) {
    anyChanged = true;
  }
#endif
  if (getFromRequest<int>(reqObj, "modeTimeout", iotsaConfig.configurationModeTimeout)) {
    anyChanged = true;
  }

#ifdef IOTSA_WITH_HTTPS
  // Set parameter defaultCert to true to remove any key/certificate
  bool defaultCert;
  if (getFromRequest<bool>(reqObj, "defaultCert", defaultCert) && defaultCert) {
    IOTSA_FS.remove("/config/httpsKey.der");
    IOTSA_FS.remove("/config/httpsCert.der");
  }
  // Allow setting of https key as PEM. Note that using POST with file upload will
  // work more often due to memory constraints and the size of keys and certificates.
  const char *b64Value = nullptr;
  if (getFromRequest<const char *>(reqObj, "httpsKey", b64Value) && b64Value)) {
    static const char *head = "-----BEGIN RSA PRIVATE KEY-----";
    static const char *tail = "-----END RSA PRIVATE KEY-----";
    char *headPos = strstr(b64Value, head);
    char *tailPos = strstr(b64Value, tail);
    if (headPos == b64Value && tailPos) {
      b64Value += strlen(head);
      *tailPos = '\0';
    } else {
      IFDEBUG IotsaSerial.println("req httpsKey not PEM");
      b64Value = "";
    }
    int b64len = strlen(b64Value);
    IFDEBUG IotsaSerial.println("req has httpsKey");
    char *tmpValue = (char *)malloc(base64_decode_expected_len(b64len));
    if (tmpValue) {
      int decodedLen = base64_decode_chars(b64Value, b64len, tmpValue);
      if (decodedLen > 0) {
        newKey = (uint8_t *)tmpValue;
        newKeyLength = decodedLen;
        anyChanged = true;
      } else {
        IFDEBUG IotsaSerial.println("could not decode httpsKey");
      }
    } else {
      IFDEBUG IotsaSerial.println("httpsKey malloc failed");
    }
  }
  // Allow setting of https certificate as PEM. Note that using POST with file upload will
  // work more often due to memory constraints and the size of keys and certificates.
  b64Value = nullptr;
  if (getFromRequest<const char *>(reqObj, "httpsCertificate", b64Value) && b64Value)) {
    const char *b64Value = reqObj["httpsCertificate"];
    static const char *head = "-----BEGIN CERTIFICATE-----";
    static const char *tail = "-----END CERTIFICATE-----";
    char *headPos = strstr(b64Value, head);
    char *tailPos = strstr(b64Value, tail);
    if (headPos == b64Value && tailPos) {
      b64Value += strlen(head);
      *tailPos = '\0';
    } else {
      IFDEBUG IotsaSerial.println("req httpsCertificate not PEM");
      b64Value = "";
    }
    int b64len = strlen(b64Value);
    IFDEBUG IotsaSerial.println("req has httpsCertificate");
    char *tmpValue = (char *)malloc(base64_decode_expected_len(b64len));
    if (tmpValue) {
      int decodedLen = base64_decode_chars(b64Value, b64len, tmpValue);
      if (decodedLen > 0) {
        newCertificate = (uint8_t *)tmpValue;
        newCertificateLength = decodedLen;
        anyChanged = true;
      } else {
        IFDEBUG IotsaSerial.println("could not decode httpsCertificate");
      }
    } else {
      IFDEBUG IotsaSerial.println("httpsCertificate malloc failed");
    }
  }
#endif // IOTSA_WITH_HTTPS
  if (anyChanged) configSave();
  if (reqObj["reboot"]) {
    iotsaConfig.requestReboot(2000);
    anyChanged = true;
  }
  return anyChanged||radioModeChanged;
}
#endif // IOTSA_WITH_API
#if defined(IOTSA_WITH_WEB)
static File _uploadFile;
static bool _uploadOK;

void
IotsaConfigMod::uploadHandler() {
  if (needsAuthentication("config")) return;
  HTTPUpload& upload = server->upload();
  _uploadOK = false;
  if(upload.status == UPLOAD_FILE_START){
    if (upload.filename != "httpsKey.der" && upload.filename != "httpsCert.der") {
      IFDEBUG IotsaSerial.println("Incorrect filename");
      return;
    }
    String _uploadfilename = "/config/" + upload.filename;
    IFDEBUG IotsaSerial.print("Uploading ");
    IFDEBUG IotsaSerial.println(_uploadfilename);
    if(IOTSA_FS.exists(_uploadfilename)) IOTSA_FS.remove(_uploadfilename);
#ifndef IOTSA_FS_OPEN_2_ARGS
    _uploadFile = IOTSA_FS.open(_uploadfilename, "w", true);
#else
    _uploadFile = IOTSA_FS.open(_uploadfilename, "w");
#endif
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(_uploadFile) {
        _uploadFile.close();
        _uploadOK = true;
    }
  }
}

void
IotsaConfigMod::uploadOkHandler() {
  String message;
  if (_uploadOK) {
    IFDEBUG IotsaSerial.println("upload ok");
    server->send(200, "text/plain", "OK");
  } else {
    IFDEBUG IotsaSerial.println("upload failed");
    server->send(403, "text/plain", "FAIL");
  }
}
#endif // defined(IOTSA_WITH_WEB) || defined(IOTSA_WITH_API)

void IotsaConfigMod::serverSetup() {
#ifdef IOTSA_WITH_WEB
  server->on("/config", std::bind(&IotsaConfigMod::handler, this));
  server->on("/config", HTTP_POST, std::bind(&IotsaConfigMod::handler, this));
  server->on("/configupload", HTTP_POST, std::bind(&IotsaConfigMod::uploadOkHandler, this), std::bind(&IotsaConfigMod::uploadHandler, this));
#endif
#ifdef IOTSA_WITH_API
  api.setup("/api/config", true, true);
  api.setup("/api/version", true);
  name = "config";
#endif
}

void IotsaConfigMod::configLoad() {
  iotsaConfig.configLoad();
}


void IotsaConfigMod::configSave() {
  iotsaConfig.configSave();
#ifdef IOTSA_WITH_HTTPS
  if (newKey && newCertificate) {
    iotsaConfigFileSaveBinary("/config/httpsKey.der", newKey, newKeyLength);
    IFDEBUG IotsaSerial.println("saved /config/httpsKey.der");
    iotsaConfigFileSaveBinary("/config/httpsCert.der", newCertificate, newCertificateLength);
    IFDEBUG IotsaSerial.println("saved /config/httpsCert.der");
  } else if (newKey || newCertificate) {
    IFDEBUG IotsaSerial.println("Not saving key/cert unless both are set");
  }
#endif // IOTSA_WITH_HTTPS
}

void IotsaConfigMod::loop() {
  if (iotsaConfig.configurationModeEndTime && millis() > iotsaConfig.configurationModeEndTime) {
    iotsaConfig.endConfigurationMode();
  }
  if (iotsaConfig.nextConfigurationModeEndTime && millis() > iotsaConfig.nextConfigurationModeEndTime) {
    iotsaConfig.endConfigurationMode();
  }
}
