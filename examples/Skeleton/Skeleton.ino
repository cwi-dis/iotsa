//
// Boilerplate for configurable web server (probably RESTful) running on ESP8266.
//
// Virgin servers come up with a private WiFi network, as host 192.168.4.1.
// You can set SSID, Password and hostname through a web interface.
// Then the server comes up on that network with the given hostname (.local)
// And you can access the applications.
//
// Look for the string CHANGE to see where you need to add things for your application.
//

#include <ESP.h>
#include "Wapp.h"
#include "WappWifi.h"

// CHANGE: Add application includes and declarations here

#define WITH_HELLO			// Define to enable "Hello world" ReST interface
#define WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_FILES  // Enable static files webserver

ESP8266WebServer server(80);
Wapplication wapplication(server, "APBoot Hello World Server");
WappWifiMod wappWifi(wapplication);

#ifdef WITH_NTP
#include "WappNtp.h"
WappNtpMod wappNtp(wapplication);
#endif

#ifdef WITH_OTA
#include "WappOta.h"
WappOtaMod wappOta(wapplication);
#endif

#ifdef WITH_HELLO
#include "WappHello.h"
WappHelloMod wappHello(wapplication);
#endif

#ifdef WITH_FILES
#include "WappFiles.h"
WappFilesMod wappFiles(wapplication);
#endif

void setup(void){
  wapplication.setup();
  wapplication.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
}
 
void loop(void){
  wapplication.loop();
}

