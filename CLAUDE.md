# iotsa

IoT device framework for ESP32/ESP8266 hardware using the Arduino framework. This repo has two components:

- **C++ library** (`src/`) — the framework itself, included by IoT applications
- **Python control tool** (`extras/python/`) — CLI for discovering, configuring, and OTA-flashing devices

## Architecture

### Module system

The framework is built from independent modules, each a `iotsa*.h/.cpp` pair in `src/`. Applications compose a device by instantiating modules and registering them with `app.addMod()` or `app.addModEarly()`.

Key modules:

- Core: `IotsaApplication`, `IotsaWifiMod`, `IotsaConfigMod`
- Auth: `IotsaUserMod`, `IotsaMultiUserMod`, `IotsaCapabilities`
- Network protocols: `IotsaApiRestMod` (REST/HTTP), `IotsaApiCoapMod` (CoAP), `IotsaApiHpsMod` (BLE/HPS)
- BLE: `IotsaBLEServerMod` (device as peripheral), `IotsaBLEClientMod` (device as central — generic
  scan/connect infrastructure, intended as a base class for app-specific modules; see
  `docs/module-interface-status.md` known issue 6)
- Hardware: `IotsaLedMod`, `IotsaButtonMod`, `IotsaInputMod`, `IotsaBatteryMod`
- Services: `IotsaOtaMod`, `IotsaNtpMod`, `IotsaRtcMod`, `IotsaLoggerMod`, `IotsaFilesMod`

### Build flags

Features are enabled/disabled at compile time. See `src/iotsaBuildOptions.h` for defaults.

Defaults (on): `IOTSA_WITH_WIFI`, `IOTSA_WITH_HTTP`, `IOTSA_WITH_WEB`, `IOTSA_WITH_REST`, `IOTSA_WITH_DEBUG`

Defaults (off, opt-in): `IOTSA_WITH_HTTPS`, `IOTSA_WITH_COAP`, `IOTSA_WITH_BLE`

Use `-DIOTSA_WITHOUT_X` to disable a default, `-DIOTSA_WITH_X` to enable an optional feature.

BLE is ESP32-only and uses NimBLE (h2zero/NimBLE-Arduino) exclusively; the old ESP32 BLE Arduino
library path was removed, see cwi-dis/iotsa#150.

## Build system

**Primary tool: PlatformIO.** Board+example combinations are defined as `[env:board-example-name]` sections in `platformio.ini`. The default env is `lolin32-example-skeleton-http-ble`.

```bash
# Build default env
pio run

# Build a specific env
pio run -e lolin32-example-hello

# Upload via USB
pio run -e lolin32-example-hello -t upload

# Monitor serial output
pio device monitor
```

The `platformio_pre_script.py` runs before each build and injects `IOTSA_CONFIG_PROGRAM_NAME`, `IOTSA_CONFIG_PROGRAM_REPO`, and `IOTSA_CONFIG_PROGRAM_VERSION` macros derived from git metadata.

Arduino IDE and arduino-cli are also supported.

## Flashing

- **First flash / wired:** `pio run -e <env> -t upload` over USB
- **Subsequent updates:** OTA via the Python control tool (see below)

### OTA workflow — order matters

Always follow this sequence: **edit → build (to verify) → commit → rebuild → flash.**

If you build before committing, the firmware embeds the previous HEAD hash as `programVersion`, so the running device reports the wrong version. The rebuild after committing is fast (only the version string changes) but important for traceability.

```bash
pio run -e <env>                              # verify it builds
git commit ...
pio run -e <env>                              # rebuild with correct hash
iotsa -t <host> otaWait ota .pio/build/<env>/firmware.bin
```

## Python control tool

Located in `extras/python/`. There is no venv in this repo — the whole `iotsa-group` shares
one venv at `~/src/iotsa-group/.venv`, with this package installed editable into it. See
`iotsa-group/CLAUDE.md`'s "Python tool venv setup" section for how to create/use it.

```bash
source ~/src/iotsa-group/.venv/bin/activate
iotsa --help
```

The tool discovers devices via mDNS, communicates over HTTP/HTTPS (REST), CoAP, or BLE/HPS, and supports OTA firmware upload, config inspection, and config editing.

## Examples and tests

`examples/` contains doc-grade sample sketches, one-to-one with the README's "sample
programs" list. `tests/` holds board/feature build-coverage variants as data only
(`iotsa-build.json`, pointing back at the `examples/` source they build) — not tutorial
material. `sandbox/` (#222) holds sketches still under active development — self-contained,
like `tests/KitchenSink`, rather than pointing back at an `examples/` source — that build on
every push (so breakage is caught immediately) but, unlike `examples/`, carry no promise of
being a stable starting point to copy, and aren't held to `examples/`'s full board/flag
coverage bar. All three feed `extras/python/gen_build_matrix.py`, the single source of truth
for the toplevel `platformio.ini` envs (`generated_envs.ini`), both CI workflows' build
matrices, and each entry's standalone `platformio.ini`. See #156.

- `Skeleton` — recommended starting point for new applications
- `Hello` — simplest possible single-file "Hello, user" server
- `HelloIotsa` — the same, but structured the way we recommend for real applications
  (app module in its own `.h`/`.cpp`), with a REST API added
- `HelloPasswd`, `HelloRights`, `HelloToken`, `HelloUser` — auth patterns
- `Led`, `Light` — LED control / BLE
- `Button`, `Input`, `Ringer` — input handling
- `Log`, `Temperature`, `DateTime` — logging and sensors
- `sandbox/BLEClient` — active-development test rig for `IotsaBLEClientMod`-based
  device-to-device communication; not in the list above since it isn't doc-grade

## CI

`.github/workflows/build-platformio.yml` builds all examples on three boards (iotsa_v4, esp32thing, esp32c3devkit) and uploads firmware binaries as artifacts.

On push to `develop` or a version tag, it dispatches rebuild events to all downstream iotsa application repos.

## Module interface status

A detailed analysis of which interfaces (web UI, REST, BLE) each module exposes, known
inconsistencies, and what to check in sibling repos is in
[`docs/module-interface-status.md`](docs/module-interface-status.md).

## Branch strategy

- `develop` — active development
- `master` — stable / release
