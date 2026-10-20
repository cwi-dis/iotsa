//
// KitchenSink: not a tutorial, a stress rig (see iotsa#151/#203). Instantiates as many
// iotsa modules together as is mutually sane, to measure worst-case flash/RAM
// consumption and to serve as a hardware test bed for module interaction bugs.
//
// This intentionally lives under tests/, not examples/: unlike every other sketch in
// this repo it isn't meant to be copied as a starting point for a real device.
//
// Deliberately excluded:
// - iotsaButton: superseded by iotsaInput (iotsa#160), and the two headers can't even
//   be included together (both define a top-level `class Button`, with different
//   bases -- a straight ODR collision, not just redundancy).
// - iotsaLedControlMod-style REST/web wrapper: that's app-specific UX layered on top
//   of iotsaLed in each example that needs it, not something iotsa itself ships.
//
// Build flags -DIOTSA_WITH_BLE -DIOTSA_WITH_COAP -DIOTSA_WITH_HTTPS (see
// tests/KitchenSink/iotsa-build.json) add BLE/CoAP/HTTPS coverage on top of the
// always-on HTTP/REST -- those axes are additive, not exclusive, so a single sketch
// can exercise all of them together.
//

#include "iotsa.h"
#include "iotsaUser.h"
#include "iotsaCapabilities.h"
#include "iotsaBattery.h"
#include "iotsaFiles.h"
#include "iotsaFilesUpload.h"
#include "iotsaFilesBackup.h"
#include "iotsaNtp.h"
#include "iotsaRtc.h"
#include "iotsaLogger.h"
#include "iotsaLed.h"
#include "iotsaInput.h"
#include "iotsaSimple.h"
#include "iotsaNothing.h"
#include "iotsaBLEClient.h"
#include "iotsaBLEClientManager.h"

#ifndef IOTSA_PIN_BUTTON
#define IOTSA_PIN_BUTTON 0 // GPIO0 is the "Boot" pin, wired to a pushbutton on most dev boards.
#endif
#ifndef IOTSA_PIN_ENCODER_A
#define IOTSA_PIN_ENCODER_A 14
#endif
#ifndef IOTSA_PIN_ENCODER_B
#define IOTSA_PIN_ENCODER_B 2
#endif

#ifdef ESP32
// ESP32-C3 and -S3 boards in this family don't have the 1:1 voltage-divider wiring
// on these pins -- gate on the chip target directly (CONFIG_IDF_TARGET_xxx, the
// ESP-IDF-provided macro, correct under both PlatformIO and Arduino IDE/arduino-cli)
// rather than via a separate opt-out build flag.
#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#define IOTSA_PIN_VBAT 36 // Undefine to disable battery voltage measurements. Use 1:1 voltage divider.
#define IOTSA_PIN_VUSB 37 // Undefine to disable USB voltage measurements. Use 1:1 voltage divider.
#endif
#ifdef CONFIG_IDF_TARGET_ESP32C3
#define IOTSA_PIN_DISABLESLEEP 9 // Define as pin to disable sleep (active low to disable)
#else
#define IOTSA_PIN_DISABLESLEEP 0 // Define as pin to disable sleep (active low to disable)
#endif
#endif // ESP32

// Real-time clock pins (DS1302). Only on the classic
// ESP32: the ESP8266, C3 and S3 don't have these pins, and on the C3 GPIO21 is the
// console's TX pin (the RTC clock on it turned the console into garbage).
#ifdef CONFIG_IDF_TARGET_ESP32
#define WITH_RTC
#define PIN_RTC_ENA 23
#define PIN_RTC_CLK 21
#define PIN_RTC_DAT 22
#endif

IotsaApplication application("Iotsa KitchenSink Test Rig");

// Multi-user auth, with capabilities chained on top. The top of that stack is the
// application's auth provider (setAuth() in setup()), used by every module.
IotsaUserMod userMod(application);
IotsaCapabilityMod capabilityMod(application, userMod);
#define authProvider (&capabilityMod)

IotsaBatteryMod batteryMod(application);
IotsaFilesMod filesMod(application);
IotsaFilesUploadMod filesUploadMod(application);
IotsaFilesBackupMod filesBackupMod(application);
IotsaNtpMod ntpMod(application);
#ifdef WITH_RTC
IotsaRtcMod rtcMod(application, PIN_RTC_ENA, PIN_RTC_CLK, PIN_RTC_DAT);
#endif
IotsaLoggerMod loggerMod(application);
// Declared explicitly (rather than auto-created) so the module is exercised on
// every board: the board's own status LED where it has one, else a NeoPixel on 15.
#ifdef IOTSA_PIN_LED
IotsaLedMod ledMod(application);
#else
IotsaLedMod ledMod(application, 15, NEO_GRB + NEO_KHZ800);
#endif
IotsaNothingMod nothingMod(application);

// iotsaInput: a rotary encoder and a pushbutton (see examples/Input for pin meaning).
// No encoder on the C3: it has no GPIO14.
Button button(IOTSA_PIN_BUTTON, true, true, true);
#ifdef CONFIG_IDF_TARGET_ESP32C3
Input* inputs[] = { &button };
#else
RotaryEncoder encoder(IOTSA_PIN_ENCODER_A, IOTSA_PIN_ENCODER_B);
Input* inputs[] = { &button, &encoder };
#endif
IotsaInputMod inputMod(application, inputs, sizeof(inputs)/sizeof(inputs[0]));

// IotsaRequest: nothing else in examples/ or tests/ instantiates it (IotsaButtonMod,
// its only other user, is excluded above) -- wire it to the button so pressing it
// exercises the outbound-request code path, self-loopback GET against nothingMod's
// own REST endpoint so it needs no external server to hit (see cwi-dis/iotsa#222).
static IotsaRequest buttonRequest;
static bool kitchenSinkButtonPressed() {
  buttonRequest.url = "http://" + WiFi.localIP().toString() + "/api/nothing";
  return buttonRequest.send();
}

// iotsaSimple: a trivial module with no state of its own, just to exercise the
// generic handler/info callback path.
static void kitchenSinkSimpleHandler() {
  application.server->send(200, "text/plain", "KitchenSink simple handler\n");
}
static String kitchenSinkSimpleInfo() {
  return "<p>See <a href=\"/kitchensink\">/kitchensink</a> (iotsaSimple test module).</p>";
}
IotsaSimpleMod simpleMod(application, "/kitchensink", kitchenSinkSimpleHandler, kitchenSinkSimpleInfo);

#ifdef IOTSA_WITH_BLE

// Subclassed only to start a scan from boot and to exercise coordinateWithServer
// (pauses/resumes the BLE server's advertising around each scan; iotsa creates
// the server automatically in BLE builds) -- the same BLE
// server/client interplay flagged as unsolved in the #113 scoping discussion.
// IotsaBLEClientManagerMod (not the bare IotsaBLEClientMod), since
// findUnknownDevices()/the devices-browsing REST/web page it exercises moved
// there (cwi-dis/iotsa#264).
class KitchenSinkBLEClientMod : public IotsaBLEClientManagerMod {
public:
  using IotsaBLEClientManagerMod::IotsaBLEClientManagerMod;
  void setup() override {
    IotsaBLEClientManagerMod::setup();
    // One scan window (scan_unknown_duration), like the "scan" button, not
    // forever: endless scanning kept toggling the BLE server's advertising,
    // which kept changing the largest free heap block (cwi-dis/iotsa#293).
    startScanUnknown();
  }
};
KitchenSinkBLEClientMod bleClientMod(application);
#endif // IOTSA_WITH_BLE

void setup(void) {
#ifdef IOTSA_PIN_VBAT
  batteryMod.setPinVBat(IOTSA_PIN_VBAT);
#endif
#ifdef IOTSA_PIN_VUSB
  batteryMod.setPinVUSB(IOTSA_PIN_VUSB);
#endif
#ifdef IOTSA_PIN_DISABLESLEEP
  batteryMod.setPinDisableSleep(IOTSA_PIN_DISABLESLEEP);
#endif
#ifdef IOTSA_WITH_BLE
  IotsaBLEClientMod::coordinateWithServer = true;
#endif
  button.setCallback(kitchenSinkButtonPressed);
  application.setAuth(authProvider);  // every module, the standard ones included, uses this
  application.setup();
  application.lateSetup();
}

void loop(void) {
  application.loop();
}
