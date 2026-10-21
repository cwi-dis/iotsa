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
  // has already run by now (cwi-dis/iotsa#106). IotsaStatus::statusColor() is
  // now polled continuously by renderers, not nudged here (cwi-dis/iotsa#176).
}

#ifdef IOTSA_WITH_HTTPS
// PEM (as pasted in the web form or sent over REST) to DER. Returns nullptr on
// success (der/derLength replaced, the old buffer freed), else why not.
static const char *_pemToDer(String pem, const char *head, const char *tail, const uint8_t *&der, size_t &derLength) {
  pem.trim();
  int tailPos = pem.indexOf(tail);
  if (!pem.startsWith(head) || tailPos < 0) return "not in PEM format";
  String b64 = pem.substring(strlen(head), tailPos);
  int b64len = b64.length();
  char *tmp = (char *)malloc(base64_decode_expected_len(b64len));
  if (!tmp) return "out of memory";
  // libb64 skips the line breaks
  int len = base64_decode_chars(b64.c_str(), b64len, tmp);
  if (len <= 0) {
    free(tmp);
    return "could not decode";
  }
  free((void *)der);
  der = (const uint8_t *)tmp;
  derLength = len;
  return nullptr;
}
static const char *_keyHead = "-----BEGIN RSA PRIVATE KEY-----";
static const char *_keyTail = "-----END RSA PRIVATE KEY-----";
static const char *_certHead = "-----BEGIN CERTIFICATE-----";
static const char *_certTail = "-----END CERTIFICATE-----";
#endif // IOTSA_WITH_HTTPS

#ifdef IOTSA_WITH_WEB
void IotsaConfigMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.argsNeedRight("config");
  if (ctx.arg("hostName", iotsaConfig.hostName, IotsaWebContext::ConfigModeOnly)) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    ctx.feedback("Rebooting to change the hostname.");
  }
  uint32_t timeout = iotsaController.modeTimeout();
  if (ctx.arg("modeTimeout", timeout, IotsaWebContext::ConfigModeOnly)) {
    iotsaController.setModeTimeout(timeout);
  }
  ctx.argCheckbox("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, IotsaWebContext::ConfigModeOnly);
#ifdef IOTSA_WITH_BLE
  ctx.argCheckbox("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, IotsaWebContext::ConfigModeOnly);
#endif
#ifdef IOTSA_WITH_HTTPS
  // The text fields aren't settings themselves: a valid key plus a valid
  // certificate are, and configSave() saves them only together.
  IotsaWebContext::Change upload(ctx, "uploadHttps", IotsaWebContext::ConfigModeOnly);
  if (upload) {
    String pem;
    if (ctx.arg("httpsKey", pem)) {
      const char *err = _pemToDer(pem, _keyHead, _keyTail, newKey, newKeyLength);
      if (err) ctx.error(String("HTTPS private key: ") + err);
    }
    pem = "";
    if (ctx.arg("httpsCertificate", pem)) {
      const char *err = _pemToDer(pem, _certHead, _certTail, newCertificate, newCertificateLength);
      if (err) ctx.error(String("HTTPS certificate: ") + err);
    }
    if (newKey && newCertificate) {
      upload.done();
    } else if (!ctx.hasFeedback()) {
      ctx.feedback("The HTTPS key and certificate are only saved together.");
    }
  }
#endif
}

void IotsaConfigMod::formHandler_fields(IotsaWebContext& ctx) {
  if (ctx.section == IotsaWebContext::Everyday) {
    ctx.showStatus(IOTSA_FS_NAME " usage", "", String(iotsaFSUsedBytes()) + " of " + String(iotsaFSTotalBytes()) + " bytes");
#ifdef IOTSA_WITH_HTTPS
    ctx.showStatus("HTTPS certificate", "defaultCert", iotsaConfig.usingDefaultCertificate() ? "factory-installed (not very secure)" : "uploaded");
#endif
    return;
  }
  if (!ctx.editable(IotsaWebContext::ConfigModeOnly)) {
    ctx << "<p>These settings can only be changed in configuration mode, see <a href='/runmode'>/runmode</a>.</p>\n";
  }
  {
    IotsaWebContext::Form form(ctx);
    ctx.showField("Hostname", "hostName", iotsaConfig.hostName, IotsaWebContext::ConfigModeOnly, "changing it reboots");
    ctx.showField("Configuration mode timeout", "modeTimeout", iotsaController.modeTimeout(), IotsaWebContext::ConfigModeOnly, "seconds");
    ctx.showCheckbox("WiFi disabled on boot", "wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, IotsaWebContext::ConfigModeOnly);
#ifdef IOTSA_WITH_BLE
    ctx.showCheckbox("BLE disabled on boot", "bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, IotsaWebContext::ConfigModeOnly);
#endif
  }
#ifdef IOTSA_WITH_HTTPS
  {
    IotsaWebContext::Form form(ctx, "Upload", "HTTPS certificate", "uploadHttps");
    ctx.showTextarea("Private key (PEM)", "httpsKey", IotsaWebContext::ConfigModeOnly, "both are needed");
    ctx.showTextarea("Certificate (PEM)", "httpsCertificate", IotsaWebContext::ConfigModeOnly);
  }
#endif
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
  // Persisted identity + knobs only. Runtime mode state moved to /api/runmode and
  // /api/status; the one-release GET mirror of currentMode / requestedMode /
  // wifiDisabled / bleDisabled / privateWifi / mdnsEnabled / bootCause / uptime /
  // fs*Bytes was dropped in cwi-dis/iotsa#243 step D. PUT still accepts the mode
  // keys and forwards them (the new-CLI-to-old-board compat surface that stays).
  reply["hostName"] = iotsaConfig.hostName;
  reply["modeTimeout"] = iotsaController.modeTimeout();
  reply["wifiDisabledOnBoot"] = iotsaConfig.wifiDisabledOnBoot;
#ifdef IOTSA_WITH_BLE
  reply["bleDisabledOnBoot"] = iotsaConfig.bleDisabledOnBoot;
#endif
  reply["program"] = app.title;
#ifdef IOTSA_WITH_HTTPS
  reply["defaultCert"] = iotsaConfig.usingDefaultCertificate();
  reply["has_httpsKey"] = IOTSA_FS.exists("/config/httpsKey.der");
  reply["has_httpsCert"] = IOTSA_FS.exists("/config/httpsCert.der");
#endif
  JsonArray modules = reply["modules"].to<JsonArray>();
  JsonArray modulesNoApi = reply["modulesNoApi"].to<JsonArray>();
  // "version" and "status" are read-only API paths, not IotsaModules -- listed here
  // so `iotsa allInfo` and anything else walking modules[] can find them. "status"
  // is served by IotsaRunmodeMod (always present, core-tier); adding it was the bit
  // #106 step 5e missed (cwi-dis/iotsa#243 step A).
  modules.add("version");
  modules.add("status");
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

  return true;
}

bool IotsaConfigMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply, bool *changed) {
  bool anyChanged = false;

  JsonObject reqObj = request.as<JsonObject>();
  // First look for arguments that are also valid in normal mode.
  // wifiDisabled / bleDisabled / requestedMode / reboot are [[deprecated]]
  // forwarders here -- canonical in /api/runmode (cwi-dis/iotsa#106). Kept for
  // one release so existing scripts and the Python CLI keep working.
  bool wifiDisabled;
  if (getFromRequest<int>(reqObj, "wifiDisabled", wifiDisabled)) {
    iotsaController.setWifiRadioEnabled(!wifiDisabled);  // cwi-dis/iotsa#106
  }
#ifdef IOTSA_WITH_BLE
  bool bleDisabled;
  if (getFromRequest<int>(reqObj, "bleDisabled", bleDisabled)) {
    iotsaController.setBleRadioEnabled(!bleDisabled);  // cwi-dis/iotsa#106
  }
#endif
  int reqModeInt;
  if (getFromRequest<int>(reqObj, "requestedMode", reqModeInt)) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106), so no
    // configSave() is needed here even though the early return below skips it.
    if (reqModeInt < IOTSA_MODE_NORMAL || reqModeInt > IOTSA_MODE_FACTORY_RESET) return apiError(400, "requestedMode: no such mode");
    iotsaController.requestMode(iotsa_mode(reqModeInt));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      reply["requestedMode"] = int(iotsaController.requestedMode());
      reply["requestedModeTimeout"] = (iotsaController.requestedModeEndTime() - millis())/1000;
      reply["needsReboot"] = true;
    }
  }
  bool reboot = false;
  getFromRequest<bool>(reqObj, "reboot", reboot);   // backward-compat forwarder: /api/runmode is canonical (cwi-dis/iotsa#106)
  if (!iotsaConfigSettingsWritable()) {
    // The fields below need configuration mode: say so, rather than silently
    // dropping them (cwi-dis/iotsa#280). Anything else is reported as ignored.
    static const char *configModeFields[] = {
      "hostName", "wifiDisabledOnBoot", "bleDisabledOnBoot", "modeTimeout",
      "defaultCert", "httpsKey", "httpsCertificate"
    };
    String needConfig;
    for (const char *f : configModeFields) {
      if (reqObj[f].isNull()) continue;
      if (needConfig.length()) needConfig += ", ";
      needConfig += f;
    }
    if (needConfig.length()) {
      needConfig += ": need configuration mode";
      return apiError(409, needConfig.c_str());
    }
    if (reboot) iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    checkUnhandled(reqObj);
    return true;
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
  const char *pem = nullptr;
  if (getFromRequest<const char *>(reqObj, "httpsKey", pem) && pem) {
    const char *err = _pemToDer(pem, _keyHead, _keyTail, newKey, newKeyLength);
    if (err) return apiError(400, (String("httpsKey: ") + err).c_str());
    anyChanged = true;
  }
  pem = nullptr;
  if (getFromRequest<const char *>(reqObj, "httpsCertificate", pem) && pem) {
    const char *err = _pemToDer(pem, _certHead, _certTail, newCertificate, newCertificateLength);
    if (err) return apiError(400, (String("httpsCertificate: ") + err).c_str());
    anyChanged = true;
  }
#endif // IOTSA_WITH_HTTPS
  if (anyChanged) {
    configSave();
    if (changed) *changed = true;
    iotsaController.extendCurrentMode();   // an edit happened -> keep the window open (5c)
    iotsaStatus.setStatusPulse(IotsaStatus::COLOUR_MAGENTA, 0, 0, 2000, "Settings saved");  // cwi-dis/iotsa#176
  }
  if (reboot) iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
  checkUnhandled(reqObj);
  return true;
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
  iotsaNoKeepAlive(app.server);  // cwi-dis/iotsa#293
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
