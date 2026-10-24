// Stand-in for the Arduino core, for host tests (tests/host/README.md): only
// what the headers under test use. millis() returns what the test sets.
#ifndef _HOST_ARDUINO_H_
#define _HOST_ARDUINO_H_
#include <stdint.h>

extern uint32_t hostMillis;
inline uint32_t millis() { return hostMillis; }

#endif
