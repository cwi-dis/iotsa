#ifndef _IOTSABLE_H_
#define _IOTSABLE_H_
#include "iotsa.h"
#include "iotsaApi.h"

// Common include file for iotsa BLE clients and servers, mainly to ensure
// we either include the Nimble headers or the classic Bluetooth stack headers
#ifdef IOTSA_WITH_BLE
#ifdef IOTSA_WITH_NIMBLE
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2904.h>
#endif
typedef const char * UUIDstring;

// Idempotent: ensures BLEDevice::init() has run exactly once, regardless of
// whether the server module, the client module, or both call it.
void iotsaBLE_ensureInitialized();

// Instrumentation: called on every advertising/scanning state transition, so
// both roles can be observed from one place while debugging radio-coexistence.
void iotsaBLE_notifyAdvertisingStateChanged(bool active);
void iotsaBLE_notifyScanningStateChanged(bool active);
#endif // IOTSA_WITH_BLE
#endif // _IOTSABLE_H