# Host tests

Unit tests that run on the build machine, not on a device, for the parts of iotsa that
don't need hardware. `Arduino.h` here is a stand-in for the Arduino core with only what
the headers under test use; its `millis()` returns whatever the test sets, so a test
can put the clock anywhere, including just before it wraps around at 2^32.

Run them all with

```
tests/host/run.sh
```

CI runs this in the `generate-matrix` job of `build-platformio.yml`.

- `test_deadline.cpp`: `IotsaDeadline` and the elapsed-time rule of
  [#235](https://github.com/cwi-dis/iotsa/issues/235), across the wrap.

This directory has no `iotsa-build.json`, so `extras/python/gen_build_matrix.py` skips
it: it is not a sketch.
