#ifndef _IOTSABLE_H_
#define _IOTSABLE_H_
#include "iotsa.h"
#include "iotsaApi.h"

// Common include file for iotsa BLE clients and servers
#ifdef IOTSA_WITH_BLE
#include <NimBLEDevice.h>
typedef const char * UUIDstring;

// Idempotent: ensures NimBLEDevice::init() has run exactly once, regardless of
// whether the server module, the client module, or both call it.
void iotsaBLE_ensureInitialized();

// Instrumentation: called on every advertising/scanning state transition, so
// both roles can be observed from one place while debugging radio-coexistence.
void iotsaBLE_notifyAdvertisingStateChanged(bool active);
void iotsaBLE_notifyScanningStateChanged(bool active);

// NimBLEDevice's client pool (m_pClients) and NimBLEServer's connected-peer
// tracking share one underlying NIMBLE_MAX_CONNECTIONS link budget -- an app
// with both roles compiled in (e.g. lissabonController) can have its
// outgoing client connections starve out an incoming maintenance connection
// on the server role, since neither side knows about the other (confirmed
// live, 2026-09-25: BLE config commands kept failing while lissabonController
// was busy chasing 5 dimmers). These two functions let the server role
// reserve a slot for itself without either module depending on the other:
// IotsaBLEServerMod calls iotsaBLE_reserveConnectionForServer() on every
// connect/disconnect (re-arming, never shortening, an existing reservation);
// IotsaBLEClientMod::canConnect() calls iotsaBLE_serverReservationActive()
// before starting a *new* outgoing connect, refusing one only once
// NimBLEDevice::getCreatedClientCount() would leave no slot spare. Existing
// connections are never interrupted. A no-op (always inactive) in an app
// with no server role compiled in.
void iotsaBLE_reserveConnectionForServer(uint32_t graceMs);
bool iotsaBLE_serverReservationActive();
#endif // IOTSA_WITH_BLE
#endif // _IOTSABLE_H