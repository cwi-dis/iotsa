//
// Test vehicle for IotsaBLEClientMod: exercises a BLE central (scanning for
// unknown devices) and a BLE peripheral (advertising via IotsaBLEServerMod)
// running together in the same device, the same dual role "control" (the
// lissabon-controller app in the sibling lissabon repo) has. Used to debug
// a real bug where the BLE scan never receives any advertisement even
// though isScanning() reports true.
//
// Build with -DCOORDINATE_WITH_SERVER to have scanning pause the server's
// advertising for the duration of each scan (IotsaBLEClientMod::coordinateWithServer);
// omit it to leave the two roles running uncoordinated (the default).
//

#include "iotsa.h"
#include "iotsaWifi.h"

IotsaApplication application("Iotsa BLE Client test");
IotsaWifiMod wifiMod(application);

#include "iotsaBLEServer.h"
#ifdef IOTSA_WITH_BLE
IotsaBLEServerMod bleserverMod(application);
#endif

#include "iotsaBLEClient.h"
#ifdef IOTSA_WITH_BLE

class IotsaBLEClientTestMod : public IotsaBLEClientMod {
public:
  using IotsaBLEClientMod::IotsaBLEClientMod;
  void setup() override;
#ifdef IOTSA_WITH_WEB
  String info() override;
#endif
};

void IotsaBLEClientTestMod::setup() {
  IotsaBLEClientMod::setup();
  // Scan continuously for any unknown BLE device from boot, so this example
  // is useful as a standalone test rig without any app-specific device list.
  findUnknownDevices(true);
}

#ifdef IOTSA_WITH_WEB
String IotsaBLEClientTestMod::info() {
  String rv = "<p>See <a href=\"/bleclient\">/bleclient</a> for the list of BLE devices seen.";
#ifdef IOTSA_WITH_REST
  rv += " Or use REST api at <a href='/api/bleclient'>/api/bleclient</a>.";
#endif
  rv += " coordinateWithServer=";
  rv += coordinateWithServer ? "true" : "false";
  rv += ".</p>";
  return rv;
}
#endif // IOTSA_WITH_WEB

IotsaBLEClientTestMod bleClientMod(application);
#endif // IOTSA_WITH_BLE

// Standard setup() method, hands off most work to the application framework
void setup(void){
#ifdef IOTSA_WITH_BLE
#ifdef COORDINATE_WITH_SERVER
  IotsaBLEClientMod::coordinateWithServer = true;
#endif
#endif
  application.setup();
  application.lateSetup();
}

// Standard loop() routine, hands off most work to the application framework
void loop(void){
  application.loop();
}
