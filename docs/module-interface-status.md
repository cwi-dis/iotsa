# Module interface status

Analysis of the external interfaces exposed by each iotsa module, and known inconsistencies.
Last reviewed: 2026-06-06.

## Interface matrix

Each module can expose up to three external interface layers:

- **Web UI** — HTML form served at a URL, handled via `handleRequest`
- **REST** — JSON get/put/post handlers via `IotsaApiMod` base class
- **BLE** — Bluetooth HPS (HTTP Proxy Service) for REST, or dedicated `bleGetHandler`/`blePutHandler`

| Module | Web UI | Web style | REST | BLE | Notes |
|---|---|---|---|---|---|
| iotsaBattery | Yes | old `+=` | get/put | Yes | Full coverage |
| iotsaBLEServer | — | — | get/put | Yes | No web UI |
| iotsaButton | Yes | old `+=` | get/put | — | |
| iotsaCapabilities | Yes | old `+=` | get/put/post | — | Auth module |
| iotsaConfigMod | Yes | old `+=` | get/put | — | BLE-on-boot toggle in web form |
| iotsaFiles | Yes | old `+=` | — | — | File browser; REST missing |
| iotsaFilesBackup | Yes | chunked | — | — | |
| iotsaFilesUpload | Yes | `server->send` | — | — | |
| iotsaInput | — | — | — | — | Driver only; no external interfaces |
| iotsaLed | — | — | — | — | Internal status LED only |
| iotsaLogger | Yes | chunked | — | — | Log viewer; REST missing |
| iotsaMultiUser | Yes | old `+=` | get/put/post | — | Auth module |
| iotsaNothing | Yes | old `+=` | get/put | — | Template/boilerplate |
| iotsaNtp | Yes | old `+=` | get/put | — | |
| iotsaOta | — | — | — | — | Only `info()`; OTA triggered via config mode |
| iotsaRequest | Yes | old `+=` | get/put | — | Uses `IotsaApiModObject` (no-path variant) |
| iotsaRtc | Yes | old `+=` | get/put | — | |
| iotsaSimple | callback | user-provided | — | — | Delegates to app |
| iotsaUser | Yes | old `+=` | get/put/post | — | Auth module |
| iotsaWifi | Yes | old `+=` | get/put | — | |

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

## Checking sibling repos

When reviewing iotsa application repos (iotsa433, lissabon, iotsaDoorOpener, etc.), apply the
same checks:

- Are custom modules using `String +=` or chunked streaming for web forms?
- Are ArduinoJSON calls v7-style (no `&` on `as<JsonObject>()`)?
- Do modules that could benefit from REST actually have it?
- Does the web form match the REST API (same fields, same names)?
