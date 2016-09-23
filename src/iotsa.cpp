#include <ESP.h>
#include <FS.h>

#include "iotsa.h"

void
IotsaApplication::addMod(IotsaMod *mod) {
  mod->nextModule = firstModule;
  firstModule = mod;
  mod->server = server;
}

void
IotsaApplication::addModEarly(IotsaMod *mod) {
  mod->nextModule = firstEarlyModule;
  firstEarlyModule = mod;
  mod->server = server;
  
}

void
IotsaApplication::setup() {
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

  IotsaMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->setup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->setup();
  }
}

void
IotsaApplication::serverSetup() {
  IotsaMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->serverSetup();
  }
  webServerSetup();
}

void
IotsaApplication::loop() {
  IotsaMod *m;
  for (m=firstEarlyModule; m; m=m->nextModule) {
  	m->loop();
  }
  for (m=firstModule; m; m=m->nextModule) {
  	m->loop();
  }
  webServerLoop();
}

void
IotsaApplication::webServerSetup() {
  server.onNotFound(std::bind(&IotsaApplication::webServerNotFoundHandler, this));
  server.on("/", std::bind(&IotsaApplication::webServerRootHandler, this));
  server.begin();
  IFDEBUG Serial.println("HTTP server started");
}

void
IotsaApplication::webServerNotFoundHandler() {
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
IotsaApplication::webServerRootHandler() {
  LED digitalWrite(led, 1);
  String message = "<html><head><title>" + title + "</title></head><body><h1>" + title + "</h1>";
  IotsaMod *m;
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
IotsaApplication::webServerLoop() {
  server.handleClient();
}

