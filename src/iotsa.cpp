#include <ESP.h>
#include <FS.h>

#include "Wapp.h"

void
Wapplication::addMod(WappMod *mod) {
  mod->nextModule = firstModule;
  firstModule = mod;
  mod->server = server;
}

void
Wapplication::addModEarly(WappMod *mod) {
  mod->nextModule = firstEarlyModule;
  firstEarlyModule = mod;
  mod->server = server;
  
}

void
Wapplication::setup() {
  LED pinMode(led, OUTPUT);
  LED digitalWrite(led, 0);
  IFDEBUG Serial.begin(115200);
  IFDEBUG Serial.println("Serial opened");
  bool ok = SPIFFS.begin();
  if (!ok) {
    IFDEBUG Serial.println("SPIFFS.begin() failed, formatting");
    ok = SPIFFS.format();
    if (!ok) {
      IFDEBUG Serial.println("SPIFFS.format() failed");
    }
    ok = SPIFFS.begin();
    if (!ok) {
      IFDEBUG Serial.println("SPIFFS.begin() after format failed");
    }
  } else {
    IFDEBUG Serial.println("SPIFFS mounted");
  }

  WappMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->setup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->setup();
  }
}

void
Wapplication::serverSetup() {
  WappMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
  webServerSetup();
}

void
Wapplication::loop() {
  WappMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->loop();
  }
  webServerLoop();
}

void
Wapplication::webServerSetup() {
  server.onNotFound(std::bind(&Wapplication::webServerNotFoundHandler, this));
  server.on("/", std::bind(&Wapplication::webServerRootHandler, this));
  server.begin();
  IFDEBUG Serial.println("HTTP server started");
}

void
Wapplication::webServerNotFoundHandler() {
 LED digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  LED digitalWrite(led, 0);
}

void
Wapplication::webServerRootHandler() {
  LED digitalWrite(led, 1);
  String message = "<html><head><title>" + title + "</title></head><body><h1>" + title + "</h1>";
  WappMod *m;
  for (m=firstModule; m; m=m->nextModule) {
    message += m->info();
  }
  for (m=firstEarlyModule; m; m=m->nextModule) {
    message += m->info();
  }
  message += "</body></html>";
  server.send(200, "text/html", message);
  LED digitalWrite(led, 0);
}

void
Wapplication::webServerLoop() {
  server.handleClient();
}

