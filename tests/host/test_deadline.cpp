// Host test of IotsaDeadline and the elapsed-time rule (cwi-dis/iotsa#235),
// including millis() wrapping around at 2^32 (after ~49.7 days of uptime).
#include "iotsaDeadline.h"
#include <stdio.h>

uint32_t hostMillis = 0;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s (millis %u)\n", __FILE__, __LINE__, #cond, (unsigned)hostMillis); failures++; } } while (0)

// A deadline armed at `start` for `span` ms, checked just before, at and after it.
static void checkSpan(uint32_t start, uint32_t span) {
  hostMillis = start;
  IotsaDeadline d;
  CHECK(!d.armed());
  CHECK(!d.expired());
  CHECK(!d.pending());
  CHECK(d.remainingMillis() == 0);

  d.arm(span);
  CHECK(d.armed());
  CHECK(d.pending());
  CHECK(!d.expired());
  CHECK(d.remainingMillis() == span);

  hostMillis = start + span - 1;
  CHECK(d.pending());
  CHECK(!d.expired());
  CHECK(!d.fired());
  CHECK(d.remainingMillis() == 1);

  hostMillis = start + span;
  CHECK(!d.pending());
  CHECK(d.expired());
  CHECK(d.remainingMillis() == 0);

  hostMillis = start + span + 1000;
  CHECK(d.expired());          // level-triggered: stays true
  CHECK(d.fired());            // edge: true once, then disarmed
  CHECK(!d.fired());
  CHECK(!d.armed());
  CHECK(!d.expired());
}

// The elapsed-time rule: millis() - startedAt >= duration.
static void checkElapsed(uint32_t start, uint32_t duration) {
  hostMillis = start;
  uint32_t startedAt = millis();
  hostMillis = start + duration - 1;
  CHECK(!(millis() - startedAt >= duration));
  hostMillis = start + duration;
  CHECK(millis() - startedAt >= duration);
}

int main() {
  const uint32_t WRAP = 0xffffffffu;
  // Ordinary, and across the wrap: armed 10 s before it, due 20 s after it.
  checkSpan(1000, 300000);
  checkSpan(WRAP - 10000, 30000);
  checkSpan(WRAP, 1);
  checkElapsed(1000, 300000);
  checkElapsed(WRAP - 10000, 30000);

  // Re-arming moves the deadline; disarm stops it.
  hostMillis = 0;
  IotsaDeadline d;
  d.arm(100);
  hostMillis = 50;
  d.arm(100);
  hostMillis = 120;
  CHECK(d.pending());
  hostMillis = 150;
  CHECK(d.expired());
  d.disarm();
  CHECK(!d.expired());
  CHECK(!d.fired());

  // armAtLeast() moves a deadline later, never earlier; also across the wrap.
  hostMillis = WRAP - 1000;
  IotsaDeadline a;
  a.armAtLeast(5000);
  CHECK(a.remainingMillis() == 5000);
  a.armAtLeast(2000);             // earlier: ignored
  CHECK(a.remainingMillis() == 5000);
  a.armAtLeast(8000);             // later: moves
  CHECK(a.remainingMillis() == 8000);
  hostMillis = WRAP + 9000u;      // 10 s later, past the wrap
  CHECK(a.expired());
  a.armAtLeast(100);              // expired: arms afresh
  CHECK(a.pending());
  CHECK(a.remainingMillis() == 100);

  // The wrong form the rule forbids, for contrast: an absolute comparison
  // across the wrap says "not yet" forever.
  hostMillis = WRAP - 10000;
  uint32_t dueAt = millis() + 30000;   // wraps to a small number
  hostMillis = WRAP - 5000;            // 5 s later, not due yet...
  CHECK(millis() > dueAt);             // ...but the absolute form says it is

  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("test_deadline: all passed\n");
  return 0;
}
