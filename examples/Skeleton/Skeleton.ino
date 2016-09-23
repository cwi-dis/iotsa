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
#include "iotsa.h"
#include "iotsaWifi.h"

// CHANGE: Add application includes and declarations here

#define WITH_HELLO			// Define to enable "Hello world" ReST interface
#define WITH_NTP    // Use network time protocol to synchronize the clock.
#define WITH_OTA    // Enable Over The Air updates from ArduinoIDE. Needs at least 1MB flash.
#define WITH_FILES  // Enable static files webserver

ESP8266WebServer server(80);
IotsaApplication application(server, "APBoot Hello World Server");
IotsaWifiMod wifiMod(application);

#ifdef WITH_NTP
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application);
#endif

#ifdef WITH_OTA
#include "iotsaOta.h"
IotsaOtaMod otaMod(application);
#endif

#ifdef WITH_HELLO
#include "iotsaHello.h"
IotsaHelloMod helloMod(application);
#endif

#ifdef WITH_FILES
#include "iotsaFiles.h"
IotsaFilesMod filesMod(application);
#endif

void setup(void){
  application.setup();
  application.serverSetup();
  ESP.wdtEnable(WDTO_120MS);
}
 
void loop(void){
  application.loop();
}

