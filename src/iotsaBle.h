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
#endif // IOTSA_WITH_BLE
#endif // _IOTSABLE_H