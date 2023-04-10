#ifndef _IOTSAHPS_H_
#define _IOTSAHPS_H_
#include "iotsa.h"
#include "iotsaApi.h"

#ifdef IOTSA_WITH_HPS
#ifndef IOTSA_WITH_API
#error IOTSA_WITH_HPS requires IOTSA_WITH_API
#endif
#ifndef IOTSA_WITH_BLE
#error IOTSA_WITH_HPS requires IOTSA_WITH_BLE
#endif
#define IotsaHpsModBaseMod IotsaApiMod

#endif // IOTSA_WITH_HPS
#endif
