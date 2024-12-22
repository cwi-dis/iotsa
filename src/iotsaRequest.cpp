#include "iotsaRequest.h"
#include <base64.h>
#ifdef ESP32
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266HTTPClient.h>
#endif

#ifdef ESP32
#define SSL_INFO_NAME "rootCA"
#else
#define SSL_INFO_NAME "fingerprint"
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
void IotsaRequest::formHandler_emptyfields(String& message) { 
  message += "Activation URL: <input name='url'><br>\n";
#ifdef ESP32
  message += "Root CA cert <i>(https only)</i>: <input name='rootCA'><br>\n";
#else
  message += "Fingerprint <i>(https only)</i>: <input name='fingerprint'><br>\n";
#endif

  message += "Bearer token <i>(optional)</i>: <input name='token'><br>\n";
  message += "Credentials <i>(optional, user:pass)</i>: <input name='credentials'><br>\n";
}

void IotsaRequest::formHandler_fields(String& message, const String& text, const String& f_name, bool includeConfig) { 
  message += "Activation URL: <input name='" + f_name +  ".url' value='";
  message += url;
  message += "'><br>\n";
#ifdef ESP32
  message += "Root CA cert <i>(https only)</i>: <input name='" + f_name + ".rootCA' value='";
  message += sslInfo;
#else
  message += "Fingerprint <i>(https only)</i>: <input name='" + f_name +  ".fingerprint' value='";
  message += sslInfo;
#endif
  message += "'><br>\n";

  message += "Bearer token <i>(optional)</i>: <input name='" + f_name + ".token' value='";
  message += token;
  message += "'><br>\n";

  message += "Credentials <i>(optional, user:pass)</i>: <input name='" + f_name + ".credentials' value='";
  message += credentials;
  message += "'><br>\n";
}

void IotsaRequest::formHandler_TH(String& message, bool includeConfig) {
  message += "<th>URL</th><th>" SSL_INFO_NAME "</th><th>credentials</th><th>token</th>";
}

void IotsaRequest::formHandler_TD(String& message, bool includeConfig) {
  message += "<td>";
  message += url;
  message += "</td><td>";
  message += sslInfo;
  message += "</td><td>";
  message += credentials;
  message += "</td><td>";
  message += token;
  message += "</td>";
}

bool IotsaRequest::formHandler_args(IotsaWebServer *server, const String& name, bool includeConfig) {
  bool any = false;
  String wtdName = name + ".url";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), url);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(url);
    any = true;
  }
  wtdName = name + "." + SSL_INFO_NAME;
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), sslInfo);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(sslInfo);
    any = true;
  }
  wtdName = name + ".credentials";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), credentials);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(credentials);
    any = true;
    }
  wtdName = name + ".token";
  if (server->hasArg(wtdName)) {
    IotsaMod::percentDecode(server->arg(wtdName), token);
    IFDEBUG IotsaSerial.print(wtdName);
    IFDEBUG IotsaSerial.print("=");
    IFDEBUG IotsaSerial.println(token);
    any = true;
  }
  return any;
}
#endif // IOTSA_WITH_WEB

bool IotsaRequest::send(const char *query) {
  bool rv = true;
  WiFiClient client;
#ifdef ESP32
  WiFiClientSecure secureClient;
#else
  BearSSL::WiFiClientSecure *secureClientPtr = NULL;
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
      secureClientPtr->setFingerprint(sslInfo.c_str());

      rv = http.begin(*secureClientPtr, _url);
#endif
    } else {
      rv = http.begin(client, _url);  
    }
    if (!rv) {
#ifndef ESP32
      if (secureClientPtr) delete secureClientPtr;
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
    http.end();
    if (code >= 200 && code <= 299) {
      IFDEBUG IotsaSerial.print(code);
      IFDEBUG IotsaSerial.print(" OK GET ");
      IFDEBUG IotsaSerial.println(_url);
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
  }
#ifndef ESP32
  if (secureClientPtr) delete secureClientPtr;
#endif
  return rv;
}

#ifdef IOTSA_WITH_API
void IotsaRequest::getHandler(JsonObject& reply) {
  reply["url"] = url;
  reply[SSL_INFO_NAME] = sslInfo;
  reply["hasCredentials"] = credentials != "";
  reply["hasToken"] = token != "";
}

bool IotsaRequest::putHandler(const JsonVariant& request) {
  if (!request.is<JsonObject>()) return false;
  bool any = false;
  const JsonObject& reqObj = request.as<JsonObject>();
  if (getFromRequest<const char *>(reqObj, "url", url)) {
    any = true;
  }
  if (getFromRequest<const char *>(reqObj, SSL_INFO_NAME, sslInfo)) {
    any = true;
  }
  if (getFromRequest<const char *>(reqObj, "credentials", credentials)) {
    any = true;
  }
  if (getFromRequest<const char *>(reqObj, "token", token)) {
    any = true;
  }
  return any;
}
#endif // IOTSA_WITH_API