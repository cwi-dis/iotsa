# Module interface status

Analysis of the external interfaces exposed by each iotsa module, and known inconsistencies.
Last reviewed: 2026-06-06.

## Interface matrix

Each module can expose up to four interface layers:

- **Web UI** — HTML form served at a URL, handled via `handleRequest`
- **REST** — JSON get/put/post handlers via `IotsaApiMod` base class
- **BLE** — Bluetooth HPS (HTTP Proxy Service) for REST, or dedicated `bleGetHandler`/`blePutHandler`
- **Config file** — persistent storage on flash, loaded at boot, saved when settings change

Config files are not an API in the network sense, but they are closely related: most fields
settable via REST or web are persisted to a config file and restored on reboot. See the
[Config file system](#config-file-system) section below for details.

| Module | Web UI | Web style | REST | BLE | Config file |
|---|---|---|---|---|---|
| iotsaBattery | Yes | old `+=` | get/put | Yes | `battery.cfg` |
| iotsaBLEServer | — | — | get/put | Yes | `bleserver.cfg` |
| iotsaButton | Yes | old `+=` | get/put | — | `buttons.cfg` |
| iotsaCapabilities | Yes | old `+=` | get/put/post | — | `capabilities.cfg` |
| iotsaConfigMod | Yes | old `+=` | get/put | — | `config.cfg` + TLS cert binaries |
| iotsaFiles | Yes | old `+=` | — | — | — |
| iotsaFilesBackup | Yes | chunked | — | — | — |
| iotsaFilesUpload | Yes | `server->send` | — | — | — |
| iotsaInput | — | — | — | — | — |
| iotsaLed | — | — | — | — | — |
| iotsaLogger | Yes | chunked | — | — | `logger.cfg` |
| iotsaMultiUser | Yes | old `+=` | get/put/post | — | `users.cfg` |
| iotsaNothing | Yes | old `+=` | get/put | — | `nothing.cfg` |
| iotsaNtp | Yes | old `+=` | get/put | — | `ntp.cfg` |
| iotsaOta | — | — | — | — | — |
| iotsaRequest | Yes | old `+=` | get/put | — | via owner (see note 4) |
| iotsaRtc | Yes | old `+=` | get/put | — | — (empty impl) |
| iotsaSimple | callback | user-provided | — | — | — |
| iotsaUser | Yes | old `+=` | get/put/post | — | `users.cfg` |
| iotsaWifi | Yes | old `+=` | get/put | — | `wifi.cfg` |

Notes on removed columns: the **BLE** column previously tracked which modules have dedicated
`bleGetHandler`/`blePutHandler`; see [known issue 5](#5-ble-access-model) for that detail.
The old **Notes** column is now encoded per-row above; the standalone notes remain in the
sections below.

## Config file system

Config files are flat key=value text files stored in the `/config/` directory on the SPIFFS or
LittleFS flash filesystem. They are not JSON.

**Mechanism:** `IotsaBaseMod` declares two virtual methods — `configLoad()` and `configSave()`.
During `app.setup()`, the framework calls `configLoad()` on every registered module; each module
calls `configSave()` internally whenever it accepts a settings change (REST put, web form submit,
or BLE write).

**API classes:**

- `IotsaConfigFileLoad(filename)` — opens a file for reading; `cf.get(key, var, default)` reads
  a typed value (int, bool, float, String, std::string).
- `IotsaConfigFileSave(filename)` — opens a file for writing; `cf.put(key, value)` writes a
  typed value. The file is flushed and closed in the destructor.
- `iotsaConfigFileExists(filename)` — predicate for optional files.
- `iotsaConfigFileLoadBinary` / `iotsaConfigFileSaveBinary` — bulk binary helpers used for TLS
  certs (`/config/httpsKey.der`, `/config/httpsCert.der`).

**IotsaModObject — embedded-object variant:** `IotsaRequest` and the per-user objects in
`IotsaMultiUser` implement `IotsaModObject` instead of `IotsaBaseMod`. Its interface is:

```cpp
bool configLoad(IotsaConfigFileLoad& cf, const String& name);
void configSave(IotsaConfigFileSave& cf, const String& name);
```

The owning module opens the config file and passes the handle in, rather than the object opening
its own file. This lets multiple logical objects (e.g., several button definitions, several users)
share a single file with name-prefixed keys.

**What is and is not persisted:**

- `iotsaRtc` implements `configLoad()`/`configSave()` but both are empty — RTC values come from
  the hardware clock or NTP at runtime and are never written to flash.
- `iotsaConfigMod` delegates to `iotsaConfig.configLoad/configSave()`, which owns `config.cfg`.
  The TLS cert binaries are also written here but as binary files, not key=value.
- Modules with no config file (`iotsaFiles`, `iotsaFilesBackup`, `iotsaFilesUpload`, `iotsaLed`,
  `iotsaOta`, `iotsaInput`, `iotsaSimple`) have no persistent settings — they use the base-class
  no-op implementations.

## Known issues

### 1. Web UI: all old-style `String +=`

Every module with a web config form builds HTML by appending to a `String message`. Only
`iotsaFilesBackup` and `iotsaLogger` use the newer chunked streaming. On large forms (iotsaConfigMod
has ~58 appends, iotsaBattery ~28) this allocates a large heap string before sending.

Candidate for modernisation but not urgent — devices have enough heap and the forms are only
served occasionally.

### 2. ArduinoJSON v7 style: one live v6-style reference

`iotsaRequest.cpp:213`: `const JsonObject& reqObj = request.as<JsonObject>();`

Takes a `const &` to the return value of `.as<JsonObject>()`. In v7 the idiomatic form is
`JsonObject reqObj = request.as<JsonObject>();` (no `&`). It likely works because `JsonObject`
is a handle type, but it is v6 idiom and should be cleaned up.

A second instance in `iotsaCapabilities.cpp:19` (`JsonArray& gotArray`) is inside `#if 0`
dead code — harmless.

### 3. Missing REST coverage

- `iotsaFiles` — file listing/management only via web form
- `iotsaLogger` — log access only via web

Both could expose REST endpoints, but there has been no need yet.

### 4. `iotsaRequest` uses a different API base class

`IotsaRequest` inherits `IotsaApiModObject` (simplified interface: `getHandler(JsonObject&)` /
`putHandler(const JsonVariant&)` — no path argument, no reply object in put). All standalone
modules use `IotsaApiMod` (path-based). This is intentional: `IotsaRequest` is a helper object
embedded in other modules, not a standalone module.

### 5. BLE access model

Only `iotsaBattery` and `iotsaBLEServer` have dedicated `bleGetHandler`/`blePutHandler`. All
other REST-capable modules are indirectly BLE-accessible via `iotsaApiHps`, which provides
Bluetooth HTTP Proxy Service and proxies REST calls over BLE without per-module BLE code.

### 6. BLE client model (device as central)

`iotsaBLEClient`/`iotsaBLEClientConnection` (`IotsaBLEClientMod`/`IotsaBLEClientConnection`)
are a different axis from the rest of this document: every other module here describes an
interface the device *exposes*; `IotsaBLEClientMod` is instead generic infrastructure for a
device acting as a BLE central — scanning for and connecting to *other* BLE peripherals. It
doesn't fit the interface matrix above as another row (its "Web UI"/"REST" surface, `/bleclient`
and `/api/bleclient`, is about the scanner's own config — scan interval/window, list of
known/unknown devices seen — not about controlling the device itself).

Generic pieces (name/address-keyed device registry, scan orchestration, service/manufacturer
filters) live entirely in `IotsaBLEClientMod`/`IotsaBLEClientConnection`; it's intended as a base
class for application-specific modules that know what a *specific* remote peripheral's GATT
layout looks like — see `BLEDimmer` in the sibling `lissabon` repo (`libLissabon/src/BLEDimmer.*`)
for the worked example: it holds a reference to `IotsaBLEClientMod` (via `addDevice`/`getDevice`)
and layers app-specific characteristic UUIDs on top.

If an app uses both `IotsaBLEServerMod` and `IotsaBLEClientMod` together (a device that's both a
BLE peripheral and a BLE central, e.g. a remote-control unit), `IotsaBLEClientMod::coordinateWithServer`
(default `false`) can be set to have `startScanning()`/`stopScanning()` pause/resume the server's
advertising for the duration of each scan, via the already-existing `IotsaBLEServerMod::pauseServer()`/
`resumeServer()`. See `examples/BLEClient/` for a standalone test rig exercising both roles together.

## Checking sibling repos

When reviewing iotsa application repos (iotsa433, lissabon, iotsaDoorOpener, etc.), apply the
same checks:

- Are custom modules using `String +=` or chunked streaming for web forms?
- Are ArduinoJSON calls v7-style (no `&` on `as<JsonObject>()`)?
- Do modules that could benefit from REST actually have it?
- Does the web form match the REST API (same fields, same names)?
- Do custom modules implement `configLoad()`/`configSave()`? All persistent settings should be
  round-tripped through a config file so they survive reboot.
- Are all REST/web-writable fields also persisted in the config file — and vice versa, are all
  config-file fields also exposed via REST or web (so they can be changed without reflashing)?
- Does any application code call `rtcMod.localHours()` / `rtcMod.localMinutes()` /
  `rtcMod.isoTime()` directly? It should not — those methods return UTC, not local time. All
  local-time access should go through `ntpMod`. (See cwi-dis/iotsa#104.)
- Does application code call `ntpMod.localHours()` / `ntpMod.localMinutes()` / `ntpMod.isoTime()`
  etc.? These wrappers date from when ESP8266 lacked a full POSIX time library. All current boards
  have `localtime()` / `strftime()` / `time()` available. Application code using the module
  methods should be migrated to standard POSIX calls; the module methods are candidates for
  deprecation in iotsaNtp itself.
- Does the repo use `IotsaWifiMod` unconditionally? It should be guarded with
  `#ifdef IOTSA_WITH_WIFI` to support BLE-only builds. (See cwi-dis/iotsa#105.)
- Does custom code call into `IotsaBatteryMod` for sleep inhibit or run-mode decisions? Note that
  `IotsaBatteryMod` is slated to be split into `IotsaRunmodeMod` (sleep/wake/CPU) and a smaller
  voltage-reading module. (See cwi-dis/iotsa#106.)
