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
- Hardware: `IotsaLedMod`, `IotsaButtonMod`, `IotsaInputMod`, `IotsaBatteryMod`
- Services: `IotsaOtaMod`, `IotsaNtpMod`, `IotsaRtcMod`, `IotsaLoggerMod`, `IotsaFilesMod`

### Build flags

Features are enabled/disabled at compile time. See `src/iotsaBuildOptions.h` for defaults.

Defaults (on): `IOTSA_WITH_WIFI`, `IOTSA_WITH_HTTP`, `IOTSA_WITH_WEB`, `IOTSA_WITH_REST`, `IOTSA_WITH_DEBUG`

Defaults (off, opt-in): `IOTSA_WITH_HTTPS`, `IOTSA_WITH_COAP`, `IOTSA_WITH_BLE`

Use `-DIOTSA_WITHOUT_X` to disable a default, `-DIOTSA_WITH_X` to enable an optional feature.

BLE is ESP32-only and uses NimBLE by default (override with `-DIOTSA_WITHOUT_NIMBLE`).

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

Located in `extras/python/`. The repo root has a `.venv` (Python 3.13) for local development.

```bash
source .venv/bin/activate
iotsa --help
```

The tool discovers devices via mDNS, communicates over HTTP/HTTPS (REST), CoAP, or BLE/HPS, and supports OTA firmware upload, config inspection, and config editing.

## Examples

`examples/` contains standalone Arduino sketches. They serve as both documentation and the de-facto test suite — CI builds all of them across multiple boards.

- `Skeleton` — recommended starting point for new applications
- `Hello`, `HelloApi`, `HelloCpp`, `HelloPasswd`, `HelloRights`, `HelloToken`, `HelloUser` — auth patterns
- `BLELed`, `Led`, `Light` — LED control
- `Button`, `Input`, `Ringer` — input handling
- `Log`, `Temperature`, `DateTime` — logging and sensors

## CI

`.github/workflows/build-platformio.yml` builds all examples on three boards (nodemcuv2, esp32thing, esp32-c3-devkitm-1) and uploads firmware binaries as artifacts.

On push to `develop` or a version tag, it dispatches rebuild events to all downstream iotsa application repos.

## Module interface status

A detailed analysis of which interfaces (web UI, REST, BLE) each module exposes, known
inconsistencies, and what to check in sibling repos is in
[`docs/module-interface-status.md`](docs/module-interface-status.md).

## Branch strategy

- `develop` — active development
- `master` — stable / release
