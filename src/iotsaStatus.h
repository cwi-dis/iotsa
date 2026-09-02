#ifndef _IOTSASTATUS_H_
#define _IOTSASTATUS_H_

// Intended to be included from iotsa.h

//
// IotsaStatus -- the read-mostly status board (cwi-dis/iotsa#106).
//
// Volatile, runtime-derived facts about what the device is doing right now:
// written (mostly every tick, by the controllers) and read by everyone. The
// opposite lifecycle to IotsaConfig, which holds identity -- persisted, and
// written only during a maintenance window. A plain data struct: no module, no
// loop(), no lifecycle. See docs/controller-architecture.md.
//
// Being split out of IotsaConfig incrementally; iotsaConfig keeps deprecated
// forwarders for one release (docs "Transition strategy").
//

class IotsaStatus {
public:
  bool wifiEnabled = false;           // WiFi radio is not disabled (NOT "connected" -- see networkIsUp())
  bool wifiStationConnected = false;  // STA has an IP
  bool wifiApActive = false;          // softAP is up, for any reason
  bool mdnsEnabled = false;           // mDNS responder is running

  bool networkIsUp();                 // reachable over the configured WiFi network (STA has an IP)
  const char *getBootReason();        // human-readable reset cause (computed once, then cached)
  void printHeapSpace();              // debug: free heap + largest block (prints on ESP32 only)
};

extern IotsaStatus iotsaStatus;
#endif
