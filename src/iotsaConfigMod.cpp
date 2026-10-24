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
  bool hostNameChanged = ctx.arg("hostName", iotsaConfig.hostName, IotsaWebContext::SavedInConfigMode);
  uint32_t timeout = iotsaController.modeTimeout();
  bool timeoutChanged = ctx.arg("modeTimeout", timeout, IotsaWebContext::SavedInConfigMode);
  ctx.argCheckbox("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, IotsaWebContext::SavedInConfigMode);
#ifdef IOTSA_WITH_BLE
  ctx.argCheckbox("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, IotsaWebContext::SavedInConfigMode);
#endif
  if (ctx.failed()) return;
  if (timeoutChanged) iotsaController.setModeTimeout(timeout);
  if (hostNameChanged) {
    iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
    ctx.feedback("Rebooting to change the hostname.");
  }
#ifdef IOTSA_WITH_HTTPS
  // The text fields aren't settings themselves: a valid key plus a valid
  // certificate are, and configSave() saves them only together.
  IotsaWebContext::Change upload(ctx, "uploadHttps", IotsaWebContext::SavedInConfigMode);
  if (upload) {
    String pem;
    if (ctx.arg("httpsKey", pem, IotsaWebContext::Saved)) {
      const char *err = _pemToDer(pem, _keyHead, _keyTail, newKey, newKeyLength);
      if (err) ctx.error(String("HTTPS private key: ") + err);
    }
    pem = "";
    if (ctx.arg("httpsCertificate", pem, IotsaWebContext::Saved)) {
      const char *err = _pemToDer(pem, _certHead, _certTail, newCertificate, newCertificateLength);
      if (err) ctx.error(String("HTTPS certificate: ") + err);
    }
    // A refused or invalid field fails the whole upload.
    if (!upload) return;
    if (newKey && newCertificate) {
      upload.done();
    } else if (!ctx.hasFeedback()) {
      ctx.feedback("The HTTPS key and certificate are only saved together.");
    }
  }
#endif
}

void IotsaConfigMod::formHandler_fields(IotsaWebContext& ctx) {
  ctx.argsNeedRight("config");
  if (ctx.includesEveryday()) {
    ctx.showStatus(IOTSA_FS_NAME " usage", "", String(iotsaFSUsedBytes()) + " of " + String(iotsaFSTotalBytes()) + " bytes");
#ifdef IOTSA_WITH_HTTPS
    ctx.showStatus("HTTPS certificate", "defaultCert", iotsaConfig.usingDefaultCertificate() ? "factory-installed (not very secure)" : "uploaded");
#endif
    return;
  }
  {
    IotsaWebContext::Form form(ctx);
    ctx.showField("Hostname", "hostName", iotsaConfig.hostName, IotsaWebContext::SavedInConfigMode, "changing it reboots");
    ctx.showField("Configuration mode timeout", "modeTimeout", iotsaController.modeTimeout(), IotsaWebContext::SavedInConfigMode, "seconds");
    ctx.showCheckbox("WiFi disabled on boot", "wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, IotsaWebContext::SavedInConfigMode);
#ifdef IOTSA_WITH_BLE
    ctx.showCheckbox("BLE disabled on boot", "bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, IotsaWebContext::SavedInConfigMode);
#endif
  }
#ifdef IOTSA_WITH_HTTPS
  {
    IotsaWebContext::Form form(ctx, "Upload", "HTTPS certificate", "uploadHttps");
    ctx.showTextarea("Private key (PEM)", "httpsKey", IotsaWebContext::SavedInConfigMode, "both are needed");
    ctx.showTextarea("Certificate (PEM)", "httpsCertificate", IotsaWebContext::SavedInConfigMode);
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

void IotsaConfigMod::getHandler(IotsaApiContext& ctx) {
  if (ctx.path() == "/api/version") {
    ctx.show("iotsaVersion", IOTSA_VERSION);
    ctx.show("iotsaFullVersion", IOTSA_FULL_VERSION);
#ifdef IOTSA_CONFIG_PROGRAM_NAME
    ctx.show("programName", IOTSA_CONFIG_PROGRAM_NAME);
#endif
#ifdef IOTSA_CONFIG_PROGRAM_VERSION
    ctx.show("programVersion", IOTSA_CONFIG_PROGRAM_VERSION);
#endif
#ifdef IOTSA_CONFIG_PROGRAM_REPO
    ctx.show("programRepo", IOTSA_CONFIG_PROGRAM_REPO);
#endif
#ifdef ARDUINO_VARIANT
    ctx.show("board", ARDUINO_VARIANT);
#elif defined(ARDUINO_BOARD)
    ctx.show("board", ARDUINO_BOARD);
#endif
    return;
  }
  // Persisted identity + knobs only. Runtime mode state moved to /api/runmode and
  // /api/status; the one-release GET mirror of currentMode / requestedMode /
  // wifiDisabled / bleDisabled / privateWifi / mdnsEnabled / bootCause / uptime /
  // fs*Bytes was dropped in cwi-dis/iotsa#243 step D. PUT still accepts the mode
  // keys and forwards them (the new-CLI-to-old-board compat surface that stays).
  ctx.show("hostName", iotsaConfig.hostName);
  ctx.show("modeTimeout", iotsaController.modeTimeout());
  ctx.show("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot);
#ifdef IOTSA_WITH_BLE
  ctx.show("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot);
#endif
  ctx.show("program", app.title);
#ifdef IOTSA_WITH_HTTPS
  ctx.show("defaultCert", iotsaConfig.usingDefaultCertificate());
  // Write-only, the certificate too: it isn't secret, but it is in the TLS
  // handshake and in /backup.tar, and too big for an HPS reply (cwi-dis/iotsa#296).
  ctx.showWriteOnly("httpsKey", IOTSA_FS.exists("/config/httpsKey.der"));
  ctx.showWriteOnly("httpsCertificate", IOTSA_FS.exists("/config/httpsCert.der"));
#endif
  JsonObject reply = ctx.reply();
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
}

void IotsaConfigMod::putHandler(IotsaApiContext& ctx) {
  // The settings, as on the web page: configuration mode and the "config" right.
  ctx.argsNeedRight("config");
  bool hostNameChanged = ctx.arg("hostName", iotsaConfig.hostName, IotsaApiContext::SavedInConfigMode);
  ctx.arg("wifiDisabledOnBoot", iotsaConfig.wifiDisabledOnBoot, IotsaApiContext::SavedInConfigMode);
#ifdef IOTSA_WITH_BLE
  ctx.arg("bleDisabledOnBoot", iotsaConfig.bleDisabledOnBoot, IotsaApiContext::SavedInConfigMode);
#endif
  uint32_t timeout = iotsaController.modeTimeout();
  bool timeoutChanged = ctx.arg("modeTimeout", timeout, IotsaApiContext::SavedInConfigMode);
#ifdef IOTSA_WITH_HTTPS
  // defaultCert: true removes any key and certificate.
  bool defaultCert = false;
  bool removeCert = ctx.arg("defaultCert", defaultCert, IotsaApiContext::SavedInConfigMode);
  // The key and certificate as PEM; configSave() saves them only together.
  String pem;
  if (ctx.argPassword("httpsKey", pem, IotsaApiContext::SavedInConfigMode)) {
    const char *err = _pemToDer(pem.c_str(), _keyHead, _keyTail, newKey, newKeyLength);
    if (err) ctx.error(400, String("httpsKey: ") + err);
  }
  pem = "";
  if (ctx.argPassword("httpsCertificate", pem, IotsaApiContext::SavedInConfigMode)) {
    const char *err = _pemToDer(pem.c_str(), _certHead, _certTail, newCertificate, newCertificateLength);
    if (err) ctx.error(400, String("httpsCertificate: ") + err);
  }
#endif // IOTSA_WITH_HTTPS
  // Backward-compat forwarders: /api/runmode is canonical (cwi-dis/iotsa#106),
  // and they get the same rights as there.
  bool wifiDisabled = !iotsaStatus.wifiEnabled;
  bool wifiChanged = ctx.arg("wifiDisabled", wifiDisabled, IotsaApiContext::Transient);
#ifdef IOTSA_WITH_BLE
  bool bleDisabled = !iotsaController.bleRadioWanted();
  bool bleChanged = ctx.arg("bleDisabled", bleDisabled, IotsaApiContext::Transient);
#endif
  bool reboot = false;
  ctx.arg("reboot", reboot, IotsaApiContext::Transient);
  ctx.argsNeedRight(IOTSA_RIGHT_NONE);
  int requestedMode = -1;
  bool requestChanged = ctx.arg("requestedMode", requestedMode, IotsaApiContext::Transient);
  if (requestChanged && (requestedMode < IOTSA_MODE_NORMAL || requestedMode > IOTSA_MODE_FACTORY_RESET)) {
    ctx.error(400, "requestedMode: no such mode");
  }

  if (ctx.failed()) return;
  if (timeoutChanged) iotsaController.setModeTimeout(timeout);
#ifdef IOTSA_WITH_HTTPS
  if (removeCert && defaultCert) {
    IOTSA_FS.remove("/config/httpsKey.der");
    IOTSA_FS.remove("/config/httpsCert.der");
  }
#endif
  if (hostNameChanged) ctx.show("needsReboot", true);
  if (wifiChanged) iotsaController.setWifiRadioEnabled(!wifiDisabled);
#ifdef IOTSA_WITH_BLE
  if (bleChanged) iotsaController.setBleRadioEnabled(!bleDisabled);
#endif
  if (requestChanged) {
    // requestMode() writes the pending-mode mailbox itself (cwi-dis/iotsa#106).
    iotsaController.requestMode(iotsa_mode(requestedMode));
    if (iotsaController.requestedMode() != iotsa_mode(0)) {
      ctx.show("requestedMode", int(iotsaController.requestedMode()));
      ctx.show("requestedModeTimeout", iotsaController.requestedModeRemainingMillis()/1000);
      ctx.show("needsReboot", true);
    }
  }
  if (reboot) iotsaController.requestReboot(IotsaController::REBOOT_DELAY_HTTP_MS);
}

void IotsaConfigMod::lateSetup() {
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
