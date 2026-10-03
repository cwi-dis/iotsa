#ifndef _IOTSABLE_H_
#define _IOTSABLE_H_
#include "iotsa.h"
#include "iotsaApi.h"

// Common include file for iotsa BLE clients and servers
#ifdef IOTSA_WITH_BLE
#include <NimBLEDevice.h>
typedef const char * UUIDstring;

// The runmode BLE protocol: service + characteristic UUIDs for IotsaRunmodeMod's
// BLE control surface (current/requested mode, reboot, promote-mode, wifiDisabled,
// identify). Moved here from IotsaRunmodeMod (where they used to be `protected`,
// invisible to anything outside that one class) so a BLE *client* -- not just the
// server that implements this protocol -- has a real, shared place to reference
// them from, instead of hardcoding raw UUID strings (which is what the Python CLI's
// bleIotsaUUIDs.py had to do, duplicated and out of sync with the C++ side). Plays
// the same role for the runmode protocol that LissabonBLE.h plays for the dimmer
// protocol. Values themselves are unchanged from IotsaRunmodeMod's originals.
namespace IotsaRunmodeBLE {
  static constexpr UUIDstring serviceUUID       = "6E5D0001-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring currentModeUUID   = "6E5D0002-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring requestedModeUUID = "6E5D0003-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring rebootUUID        = "6E5D0004-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring promoteModeUUID   = "6E5D0005-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring wifiDisabledUUID  = "6E5D0006-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
  static constexpr UUIDstring identifyUUID      = "6E5D0007-F2A7-4E7A-9B1C-2D3E4F5A6B7C";
};

// Idempotent: ensures NimBLEDevice::init() has run exactly once, regardless of
// whether the server module, the client module, or both call it.
void iotsaBLE_ensureInitialized();

// Instrumentation: called on every advertising/scanning state transition, so
// both roles can be observed from one place while debugging radio-coexistence.
void iotsaBLE_notifyAdvertisingStateChanged(bool active);
void iotsaBLE_notifyScanningStateChanged(bool active);

// The single coordination point for BLE radio contention (cwi-dis/iotsa#263):
// scan-vs-connect exclusion lives directly in IotsaBLEClientMod (it's purely
// internal to that one class), but everything that has to be arbitrated
// *across* modules -- client vs. server, BLE vs. WiFi -- goes through here.
// Static methods only, no instance: there is exactly one physical radio per
// device, so there is never a reason to have more than one of these.
class IotsaBLERadioArbiter {
public:
  // NimBLEDevice's client pool (m_pClients) and NimBLEServer's connected-peer
  // tracking share one underlying NIMBLE_MAX_CONNECTIONS link budget -- an app
  // with both roles compiled in (e.g. lissabonController) can have its
  // outgoing client connections starve out an incoming maintenance connection
  // on the server role, since neither side knows about the other (confirmed
  // live, 2026-09-25: BLE config commands kept failing while lissabonController
  // was busy chasing 5 dimmers). These two methods let the server role
  // reserve a slot for itself without either module depending on the other:
  // IotsaBLEServerMod calls reserveConnectionForServer() on every
  // connect/disconnect (re-arming, never shortening, an existing reservation);
  // IotsaBLEClientMod::canConnect() calls serverReservationActive()
  // before starting a *new* outgoing connect, refusing one only once
  // NimBLEDevice::getCreatedClientCount() would leave no slot spare. Existing
  // connections are never interrupted. A no-op (always inactive) in an app
  // with no server role compiled in.
  static void reserveConnectionForServer(uint32_t graceMs);
  static bool serverReservationActive();

  // Lets WiFi-heavy work that doesn't want BLE radio contention (OTA transfers
  // especially) tell the BLE side to hold off starting anything new for a
  // while. ESP32's WiFi/BT coexistence scheduling already time-slices the two
  // radios at a low level, but that doesn't prevent our own application-level
  // BLE work (a scan, a new outgoing connect) from making a slow OTA transfer
  // slower, or the reverse -- this is a cooperative signal, not a hardware
  // guarantee. Same rule as every other check in this arbiter: only ever
  // blocks *new* scans/connects from starting (IotsaBLEClientMod::canConnect()/
  // updateScanning()); never interrupts one already in progress. Caller (e.g.
  // IotsaOtaMod) is responsible for pairing every true with a matching false --
  // there is no timeout/grace-period auto-clear here, unlike the server
  // reservation above, since OTA already has its own onEnd()/onError() hooks
  // to do that reliably.
  static void holdOffNewWork(bool hold);
  static bool newWorkHeldOff();

  // Advertising is a derived state, not something callers switch on and off
  // directly (cwi-dis/iotsa#263, ex-#208 Part A). Anyone who needs it off for
  // a while adds a reason, and removes it again when done; IotsaBLEServerMod
  // advertises only while BLE is enabled, iotsaController.bleRadioWanted()
  // says so, and no reason is set. Replaces IotsaBLEServerMod::pauseServer()/
  // resumeServer(), whose callers each started advertising again regardless
  // of what the others (or the policy) wanted.
  enum AdvertisingPauseReason : uint8_t {
    PAUSE_SLEEP = 1,        // light sleep (IotsaRunmodeMod's sleep executor)
    PAUSE_SCAN = 2,         // client scan, IotsaBLEClientMod::coordinateWithServer
    PAUSE_GATT_BUILD = 4,   // GATT table being built (IotsaBleApiService::setup() until lateSetupDone())
  };
  // Both re-evaluate advertising right away (through the reconciler below), so
  // a pause has taken effect when pauseAdvertising() returns -- the sleep
  // executor relies on that. Main task only. durationMs: when this resume ends
  // up starting advertising, stop again after that long (0: indefinitely) --
  // used for the light-sleep wake window.
  static void pauseAdvertising(AdvertisingPauseReason reason);
  static void resumeAdvertising(AdvertisingPauseReason reason, uint32_t durationMs = 0);
  static uint8_t advertisingPauseReasons() { return s_advertisingPauseReasons; }
  // Installed by IotsaBLEServerMod. Stays null in an app without a server
  // role, where pause/resume just record the reasons.
  typedef void (*AdvertisingReconciler)(uint32_t durationMs);
  static void setAdvertisingReconciler(AdvertisingReconciler reconciler) { s_advertisingReconciler = reconciler; }
private:
  static uint32_t s_serverReservedUntilMillis;
  static bool s_holdOffNewBLEWork;
  static uint8_t s_advertisingPauseReasons;
  static AdvertisingReconciler s_advertisingReconciler;
};
#endif // IOTSA_WITH_BLE
#endif // _IOTSABLE_H