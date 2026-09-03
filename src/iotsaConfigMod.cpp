#include <Esp.h>
// mDNS and the reset-reason / anti-tamper platform headers used to be needed
// here; mDNS is IotsaWifiMod's, and the reset-reason handling moved to
// IotsaController::begin() / IotsaStatus::getBootReason() (cwi-dis/iotsa#106).

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
  IFDEBUG IotsaSerial.println(iotsaStatus.getBootReason());
  iotsaConfig.setDefaultCertificate();
  configLoad();
  // The pending-mode mailbox, the boot anti-tamper gate and the factory-reset
  // trigger moved to IotsaController::begin(), which IotsaApplication::setup()
  // has already run by now (cwi-dis/iotsa#106).
  if (app.status) app.status->showStatus();
}

#ifdef IOTSA_WITH_WEB
void
IotsaConfigMod::webHandler() {
  bool wrongMode = false;
  bool anyChanged = false;
  bool hostnameChanged = false;
  if( api.webService->server->hasArg("hostName")) {
    String argValue = api.webService->server->arg("hostName");
    if (argValue != iotsaConfig.hostName) {
      if (iotsaConfigSettingsWritable()) {
        if (needsAuthentication("config")) return;
        iotsaConfig.hostName = argValue;
        anyChanged = true;
        hostnameChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  if( api.webService->server->hasArg("rebootTimeout")) {
    int newValue = api.webService->server->arg("rebootTimeout").toInt();
    if ((uint32_t)newValue != iotsaController.modeTimeout()) {
      if (iotsaConfigSettingsWritable()) {
        if (needsAuthentication("config")) return;
        iotsaController.setModeTimeout(newValue);
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
  // Mode requests + factory-reset moved to IotsaRunmodeMod / the /runmode page
  // (cwi-dis/iotsa#106).
#ifdef IOTSA_WITH_HTTPS
  if (api.webService->server->hasArg("httpsKey") && api.webService->server->arg("httpsKey") != "") {
    if (iotsaConfigSettingsWritable()) {
      if (needsAuthentication("config")) return;
      String b64String(api.webService->server->arg("httpsKey"));
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
  if (api.webService->server->hasArg("httpsCertificate") && api.webService->server->arg("httpsCertificate") != "") {
    if (iotsaConfigSettingsWritable()) {
      if (needsAuthentication("config")) return;
      String b64String(api.webService->server->arg("httpsCertificate"));
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
 if( api.webService->server->hasArg("wifiDisabledOnBoot")) {
    int newValue = api.webService->server->arg("wifiDisabledOnBoot").toInt();
    if ((bool)newValue != iotsaConfig.wifiDisabledOnBoot) {
      if (iotsaConfigSettingsWritable()) {
        if (needsAuthentication("config")) return;
        iotsaConfig.wifiDisabledOnBoot = (bool)newValue;
        anyChanged = true;
      } else {
        wrongMode = true;
      }
    }
  }
#ifdef IOTSA_WITH_BLE
 if( api.webService->server->hasArg("bleDisabledOnBoot")) {
    int newValue = api.webService->server->arg("bleDisabledOnBoot").toInt();
    if ((bool)newValue != iotsaConfig.bleDisabledOnBoot) {
      if (iotsaConfigSettingsWritable()) {
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
    iotsaController.extendCurrentMode();   // an edit happened -> keep the window open (was inConfigurationMode(true), 5c)
    message += "<p>Settings saved to Flash.</p>";
    if (hostnameChanged) {
      message += "<p><em>Rebooting device to change hostname</em>.</p>";
    }
  }
  if (!iotsaController.inConfigurationMode()) {
    message += "<p>Hostname: ";
    message += htmlEncode(iotsaConfig.hostName);
    message += " (goto configuration mode to change)<br>Configuration mode timeout: ";
    message += String(iotsaController.modeTimeout());
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
    message += "<p>" IOTSA_FS_NAME " usage: ";
    message += String(iotsaFSUsedBytes());
    message += " / ";
    message += String(iotsaFSTotalBytes());
    message += " bytes</p>";
#ifdef IOTSA_WITH_HTTPS
    if (iotsaConfig.usingDefaultCertificate()) {
      message += "<p>Using factory-installed (<b>not very secure</b>) https certificate</p>";
    } else {
      message += "<p>Using uploaded https certificate.</p>";
    }
#endif // IOTSA_WITH_HTTPS
  }
  message += "<form method='post'>";
  if (iotsaConfigSettingsWritable()) {
    message += "Hostname: <input name='hostName' value='";
    message += htmlEncode(iotsaConfig.hostName);
    message += "'><br>";
  }
  if (iotsaConfigSettingsWritable()) {
    message += "Configuration mode timeout: <input name='rebootTimeout' value='";
    message += String(iotsaController.modeTimeout());
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
  message += "<input type='submit'></form>";
  message += "<p>Mode requests (configuration / OTA / factory-reset) moved to <a href=\"/runmode\">/runmode</a>.</p>";
  message += "</body></html>";
  api.webService->server->send(200, "text/html", message);
  if (hostnameChanged) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
  }
}

String IotsaConfigMod::info() {
  String message;
  // The mode-status blurb (in configuration mode / special mode requested / ...)
  // moved to IotsaRunmodeMod::info() (cwi-dis/iotsa#106).
  message += "<p>" + app.title + " is based on iotsa " + IOTSA_FULL_VERSION + ". See <a href=\"/config\">/config</a> to change configuration.<br>";
  message += "Last boot " + String((int)millis()/1000) + " seconds ago, reason ";
  message += iotsaStatus.getBootReason();
  message += ".</p>";
  return message;
}
#endif // IOTSA_WITH_WEB

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
  reply["modeTimeout"] = iotsaController.modeTimeout();
  // currentMode / currentModeTimeout / requestedMode / requestedModeTimeout /
  // wifiDisabled are [[deprecated]] forwarders: canonical in /api/runmode now,
  // kept here for one release so existing scripts and the Python CLI keep
  // working (cwi-dis/iotsa#106, docs/controller-architecture.md).
  reply["currentMode"] = int(iotsaController.currentMode());
  if (iotsaController.currentMode()) {
    reply["currentModeTimeout"] = (iotsaController.currentModeEndTime() - millis())/1000;
  }
  reply["privateWifi"] = iotsaStatus.wifiApActive && !iotsaStatus.wifiStationConnected;
  reply["mdnsEnabled"] = iotsaStatus.mdnsEnabled;
  reply["requestedMode"] = int(iotsaController.requestedMode());
  if (iotsaController.requestedMode()) {
    reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
  }
  reply["wifiDisabled"] = !iotsaStatus.wifiEnabled;
  reply["wifiDisabledOnBoot"] = iotsaConfig.wifiDisabledOnBoot;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabled"] = !iotsaController.bleRadioWanted();   // deprecated forwarder, canonical in /api/runmode
  reply["bleDisabledOnBoot"] = iotsaConfig.bleDisabledOnBoot;
#endif
  reply["program"] = app.title;
#ifdef IOTSA_WITH_HTTPS
  reply["defaultCert"] = iotsaConfig.usingDefaultCertificate();
  reply["has_httpsKey"] = IOTSA_FS.exists("/config/httpsKey.der");
  reply["has_httpsCert"] = IOTSA_FS.exists("/config/httpsCert.der");
#endif
  reply["bootCause"] = iotsaStatus.getBootReason();
  reply["uptime"] = millis() / 1000;
  reply["fsTotalBytes"] = iotsaFSTotalBytes();
  reply["fsUsedBytes"] = iotsaFSUsedBytes();
  JsonArray modules = reply["modules"].to<JsonArray>();
  JsonArray modulesNoApi = reply["modulesNoApi"].to<JsonArray>();
  modules.add("version");
  for (IotsaBaseModule *m=app.firstEarlyModule; m; m=m->nextModule) {
    if (m->name == "") continue;
    if (m->hasApi()) modules.add(m->name); else modulesNoApi.add(m->name);
  }
  for (IotsaBaseModule *m=app.firstModule; m; m=m->nextModule) {
    if (m->name == "") continue;
    if (m->hasApi()) modules.add(m->name); else modulesNoApi.add(m->name);
  }
  JsonArray features = reply["features"].to<JsonArray>();
#ifdef IOTSA_WITH_WIFI
  features.add("wifi");
#endif
#ifdef IOTSA_WITH_HTTP
  features.add("http");
#endif
#ifdef IOTSA_WITH_HTTPS
  features.add("https");
#endif
#ifdef IOTSA_WITH_WEB
  features.add("web");
#endif
#ifdef IOTSA_HAS_COAPSERVER
  features.add("coap");
#endif
#ifdef IOTSA_HAS_HPSSERVER
  features.add("hps");
#endif
#ifdef IOTSA_WITH_BLE
  features.add("ble");
#endif
  features.add("littlefs");
  if (iotsaStatus.mdnsEnabled) features.add("mdns");

  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  bool anyChanged = false;
  bool radioModeChanged = false;

  JsonObject reqObj = request.as<JsonObject>();
  // First look for arguments that are also valid in normal mode.
  // wifiDisabled / bleDisabled / requestedMode / reboot are [[deprecated]]
  // forwarders here -- canonical in /api/runmode (cwi-dis/iotsa#106). Kept for
  // one release so existing scripts and the Python CLI keep working.
  bool wifiDisabled;
  if (getFromRequest<int>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);  // cwi-dis/iotsa#106
    radioModeChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<int>(reqObj, "bleDisabled", bleDisabled)) {
    iotsaController.setBleRadioEnabled(!bleDisabled);  // cwi-dis/iotsa#106
    radioModeChanged = true;
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106), so no
    // configSave() is needed here even though the early return below skips it.
    iotsaController.requestMode(iotsa_mode(reqModeInt));
    anyChanged = iotsaController.requestedMode() != iotsa_mode(0);
    if (anyChanged) {
      reply["requestedMode"] = int(iotsaController.requestedMode());
      reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  if (!iotsaConfigSettingsWritable()) {
    if (checkUnhandled(reqObj)) {
      IotsaSerial.println("Unhandled IotsaApi parameters, not in config mode");
    }
    if (reqObj["reboot"]) {
      // Backward-compat forwarder: /api/runmode is canonical (cwi-dis/iotsa#106).
      iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
      anyChanged = true;
    }
    return anyChanged||radioModeChanged;
  }
  if (getFromRequest<const char *>(reqObj, "hostName", iotsaConfig.hostName)) {
    anyChanged = true;
    reply["needsReboot"] = true;
  }
  if (getFromRequest<int>(reqObj, "wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot)) {
    anyChanged = true;
  }
#ifdef IOTSA_WITH_BLE
  if (getFromRequest<int>(reqObj, "bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot)) {
    anyChanged = true;
  }
#endif
  { int t; if (getFromRequest<int>(reqObj, "modeTimeout", t)) { iotsaController.setModeTimeout(t); anyChanged = true; } }

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
  if (getFromRequest<const char *>(reqObj, "httpsKey", b64Value) && b64Value) {
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
  if (getFromRequest<const char *>(reqObj, "httpsCertificate", b64Value) && b64Value) {
    // (was a redundant inner `const char *b64Value = reqObj["httpsCertificate"];`
    // shadowing the just-fetched one -- the httpsKey branch above doesn't do that)
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
  if (anyChanged) {
    configSave();
    iotsaController.extendCurrentMode();   // an edit happened -> keep the window open (5c)
  }
  if (reqObj["reboot"]) {
    // Backward-compat forwarder: /api/runmode is canonical (cwi-dis/iotsa#106).
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    anyChanged = true;
  }
  if (checkUnhandled(reqObj)) {
    IotsaSerial.println("Unhandled IotsaApi parameters");
  }
  return anyChanged||radioModeChanged;
}
#if defined(IOTSA_HAS_WEBSERVER)
// Raw multipart upload, not a rendered page -- needs only an HTTP transport, not
// IOTSA_WITH_WEB's web UI. See cwi-dis/iotsa#205 (this exact conflation was the
// issue's original motivating bug) and cwi-dis/iotsa#221 (this duplicates
// IotsaFilesUploadMod, fixed the same way there).
static File _uploadFile;
static bool _uploadOK;

void
IotsaConfigMod::uploadHandler() {
  if (needsAuthentication("config")) return;
  HTTPUpload& upload = app.server->upload();
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
    app.server->send(200, "text/plain", "OK");
  } else {
    IFDEBUG IotsaSerial.println("upload failed");
    app.server->send(403, "text/plain", "FAIL");
  }
}
#endif // defined(IOTSA_HAS_WEBSERVER)

void IotsaConfigMod::lateSetup() {
#ifdef IOTSA_HAS_WEBSERVER
  // Two-callback upload shape (a completion handler plus a streaming upload
  // handler): not something a plain api.setup() call can express, see
  // cwi-dis/iotsa#213 -- registered directly via app.server instead, same as the
  // web-server-extension modules (this upload machinery really is that category,
  // just bolted onto the config API module -- near-duplicate of
  // IotsaFilesUploadMod, see cwi-dis/iotsa#221 for splitting it out properly).
  app.server->on("/configupload", HTTP_POST, std::bind(&IotsaConfigMod::uploadOkHandler, this), std::bind(&IotsaConfigMod::uploadHandler, this));
#endif
  api.setup("config", true, true);
  api.setup("version", true);
  name = "config";
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
  // Mode auto-expiry moved to IotsaController::tick() (cwi-dis/iotsa#106).
}
