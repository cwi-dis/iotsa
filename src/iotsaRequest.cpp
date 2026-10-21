#include "iotsaRequest.h"
#include <base64.h>
#ifdef ESP32
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266HTTPClient.h>
#endif

#define SSL_INFO_NAME "rootCA"

#ifndef ESP32
// BearSSL::WiFiClientSecure defaults to a 16KB receive buffer, which alone
// can exceed all the free heap available on ESP8266 for many real-world
// requests (see cwi-dis/iotsa#198). Extract just the host from a URL so we
// can probe whether the server supports a much smaller max fragment length
// (MFLN) before committing to shrinking the buffers -- if it doesn't, we
// fall back to the library default rather than risk a broken connection.
static String _hostFromUrl(const String &url) {
  int start = url.indexOf("://");
  start = (start < 0) ? 0 : start + 3;
  int end = url.indexOf('/', start);
  if (end < 0) end = url.length();
  String hostPort = url.substring(start, end);
  int colon = hostPort.indexOf(':');
  if (colon >= 0) hostPort = hostPort.substring(0, colon);
  return hostPort;
}
#endif


bool IotsaRequest::configLoad(IotsaConfigFileLoad& cf, const String& f_name) {
    cf.get(f_name + ".url", url, "");
    cf.get(f_name + "." + SSL_INFO_NAME, sslInfo, "");
    cf.get(f_name + ".credentials", credentials, "");
    cf.get(f_name + ".token", token, "");
    return url != "";
}

void IotsaRequest::configSave(IotsaConfigFileSave& cf, const String& f_name) {
    cf.put(f_name + ".url", url);
    cf.put(f_name + "." + SSL_INFO_NAME, sslInfo);
    cf.put(f_name + ".credentials", credentials);
    cf.put(f_name + ".token", token);
}

#ifdef IOTSA_WITH_WEB
void IotsaRequest::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("url", url, IotsaWebContext::Saved);
  ctx.argPassword(SSL_INFO_NAME, sslInfo, IotsaWebContext::Saved, false, true);
  ctx.argPassword("credentials", credentials, IotsaWebContext::Saved, false, true);
  ctx.argPassword("token", token, IotsaWebContext::Saved, false, true);
}

void IotsaRequest::formHandler_fields(IotsaWebContext& ctx) {
  ctx.showField("URL", "url", url, IotsaWebContext::Saved);
  ctx.showTextarea("Root CA certificate (https only, PEM)", SSL_INFO_NAME, IotsaWebContext::Saved, sslInfo == "" ? "not set" : "set, empty: unchanged", sslInfo != "");
  ctx.showPassword("Credentials (user:pass)", "credentials", IotsaWebContext::Saved, credentials == "" ? "optional" : "set, empty: unchanged", credentials != "");
  ctx.showPassword("Bearer token", "token", IotsaWebContext::Saved, token == "" ? "optional" : "set, empty: unchanged", token != "");
}

void IotsaRequest::formHandler_row(IotsaWebContext& ctx) {
  ctx.showCell("URL", url);
  ctx.showCell("Root CA", sslInfo, IotsaWebContext::Secret);
  ctx.showCell("Credentials", credentials, IotsaWebContext::Secret);
  ctx.showCell("Token", token, IotsaWebContext::Secret);
}
#endif // IOTSA_WITH_WEB

bool IotsaRequest::send(const char *query, String *responseBody) {
  bool rv = true;
  WiFiClient client;
#ifdef ESP32
  WiFiClientSecure secureClient;
#else
  BearSSL::WiFiClientSecure *secureClientPtr = NULL;
  BearSSL::X509List *trustAnchorPtr = NULL;
#endif
  String _url = url;
  {
    // We start a new scope, because we want to delete the
    // HTTPClient before the client/secureClient/secureClientPtr
    // so we don't have a dangling reference.
    HTTPClient http;
    if (query != NULL && *query != '\0') {
      if (_url.indexOf('?') > 0) {
        _url = _url + "&" + query;
      } else {
        _url = _url + "?" + query;
      }
    }
    if (_url.startsWith("https:")) {
#ifdef ESP32
      secureClient.setCACert(sslInfo.c_str());
      rv = http.begin(secureClient, _url);
#else
      secureClientPtr = new BearSSL::WiFiClientSecure();
      trustAnchorPtr = new BearSSL::X509List(sslInfo.c_str());
      secureClientPtr->setTrustAnchors(trustAnchorPtr);
      {
        const uint16_t fragLen = 512;
        String host = _hostFromUrl(_url);
        if (BearSSL::WiFiClientSecure::probeMaxFragmentLength(host, 443, fragLen)) {
          secureClientPtr->setBufferSizes(fragLen, fragLen);
        }
        // else: server doesn't support that fragment length, leave the
        // library default (16KB receive / 512B transmit) in place.
      }

      rv = http.begin(*secureClientPtr, _url);
#endif
    } else {
      rv = http.begin(client, _url);  
    }
    if (!rv) {
#ifndef ESP32
      if (secureClientPtr) delete secureClientPtr;
      if (trustAnchorPtr) delete trustAnchorPtr;
#endif
      return false;
    }
    // We are going to discard the http object anyway, so reuse is useless.
    http.setReuse(false);
    if (token != "") {
      http.addHeader("Authorization", "Bearer " + token);
    }

    if (credentials != "") {
      String cred64 = base64::encode(credentials);
      http.addHeader("Authorization", "Basic " + cred64);
    }
    int code = http.GET();
    if (code >= 200 && code <= 299) {
      IFDEBUG IotsaSerial.print(code);
      IFDEBUG IotsaSerial.print(" OK GET ");
      IFDEBUG IotsaSerial.println(_url);
      if (responseBody != NULL) {
        *responseBody = http.getString();
      }
    } else {
      IFDEBUG IotsaSerial.print(code);
      IFDEBUG IotsaSerial.print(" FAIL GET ");
      IFDEBUG IotsaSerial.print(_url);
#if 0
      if (sslInfo != "") {
#ifdef ESP32
        IFDEBUG IotsaSerial.print(", RootCA ");
#else
        IFDEBUG IotsaSerial.print(", fingerprint ");
#endif
        IFDEBUG IotsaSerial.print(sslInfo);
      }
#endif
      IFDEBUG IotsaSerial.println();

      rv = false;
    }
    http.end();
  }
#ifndef ESP32
  if (secureClientPtr) delete secureClientPtr;
  if (trustAnchorPtr) delete trustAnchorPtr;
#endif
  return rv;
}

bool IotsaRequest::getHandler(JsonObject& reply) {
  reply["url"] = url;
  reply[SSL_INFO_NAME] = sslInfo;
  reply["hasCredentials"] = credentials != "";
  reply["hasToken"] = token != "";
  return true;
}

bool IotsaRequest::putHandler(const JsonVariant& request, JsonObject& reply, bool *changed) {
  if (!request.is<JsonObject>()) return apiError(400, "expected an object");
  const JsonObject& reqObj = request.as<JsonObject>();
  bool any = false;
  any |= getFromRequest<const char *>(reqObj, "url", url);
  any |= getFromRequest<const char *>(reqObj, SSL_INFO_NAME, sslInfo);
  any |= getFromRequest<const char *>(reqObj, "credentials", credentials);
  any |= getFromRequest<const char *>(reqObj, "token", token);
  if (any && changed) *changed = true;
  return true;
}