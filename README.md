# iotsa - Internet of Things server architecture

### Build Status:

[![build-platformio](https://github.com/cwi-dis/iotsa/actions/workflows/build-platformio.yml/badge.svg?branch=develop)](https://github.com/cwi-dis/iotsa/actions/workflows/build-platformio.yml)
[![build-arduino](https://github.com/cwi-dis/iotsa/actions/workflows/build-arduino.yml/badge.svg?branch=develop)](https://github.com/cwi-dis/iotsa/actions/workflows/build-arduino.yml)

> See _More projects using iotsa_ section, below, for more build status indicators.

This library contains a framework to easily create esp8266-based or esp32-based web servers that can interface to all sorts of sensors and actuators. The servers can be REST-compatible, and COAP-compatible, including
seamless integration with the [Igor home automation server](https://github.com/cwi-dis/igor). 


Home page is <https://github.com/cwi-dis/iotsa>. 
This software is licensed under the [MIT license](LICENSE.txt) by the CWI DIS group, <http://www.dis.cwi.nl>.

Note that the build badges above are for the [develop branch](https://github.com/cwi-dis/iotsa/tree/develop), for the iotsa library itself and the examples. Build status for iotsa applications against the develop branch can be found below.

## Choosing hardware

For new projects, the recommended hardware is any modern **ESP32-based board**. Pick a board that suits your project's form factor and power requirements and use the matching `[env:...]` in `platformio.ini`. Esp32 variants (like esp32c3) are somewhat supported, see the table below.

**Flash size and OTA:** Over-the-air programming requires enough flash to hold two copies of the firmware plus the LittleFS filesystem. A full iotsa app doesn't fit the stock 1.25 MB app slots of a 4 MB ESP32, so iotsa's board definitions use the `min_spiffs.csv` partition layout (1.875 MB app slots, 192 KB LittleFS) for every 4 MB ESP32 board (#245). In the Arduino IDE, select the "Minimal SPIFFS" partition scheme yourself. Changing the layout of a deployed device needs a USB reflash; OTA can't do it. If firmware still doesn't fit:

- Use `no_ota.csv` — gives the full flash to a single firmware image, disabling OTA entirely.
- Reduce firmware size by excluding optional modules (e.g. omitting BLE or HTTPS).

The custom **iotsa board** (an ESP8266/ESP-12 based PCB) remains a good choice in certain situations:

- **Custom hardware integration:** The board includes an experiment area large enough for a DIP-20 IC and discrete components, making it easy to add custom circuitry (a solenoid driver, relay, RFID reader, etc.) without a separate perfboard.
- **Reliable wired programming fallback:** The board has a 6-pin FTDI header plus Reset and Program buttons, so you can always reprogram it over USB even if OTA fails.
- **Existing deployed projects:** Several iotsa applications (such as _iotsaDoorOpener_ and _iotsaNeoClock_) were designed for this board and use its experiment area.

If you have an iotsa board, see [docs/gettingStarted.md](docs/gettingStarted.md) for hardware-specific setup instructions.

### Supported hardware variants

Chip families below, ordered from most to least capable for general use. For the
individual boards iotsa is built and tested against (PlatformIO ids, Arduino
FQBNs, partition schemes, per-board quirks) see **[`docs/boards.md`](docs/boards.md)**,
generated from [`iotsa-board.json`](iotsa-board.json).

| Hardware | Flash / OTA | Status | Notes |
|---|---|---|---|
| ESP32 (original, Xtensa) | 4 MB. OTA works with `min_spiffs.csv` (the default for these boards), BLE included. | **Supported** | Tested in CI (esp32thing, canonical -- `vanilla_esp32`, #222). Lolin32 also tracked in CI via `examples/BLEManager` and `sandbox/Infra`. Pico32, ESP32dev also known to work but not continuously tested. Full features: BLE, sleep/wakeup, touch, rotary encoder. |
| ESP32-C3 (RISC-V) | 4 MB (devkit, supermini): OTA works with `min_spiffs.csv`. LCD board variant has 2 MB and is very tightly constrained. | **Supported** | Tested in CI (esp32-c3-devkitm-1). Limitations: no analog voltage reading, no deep-sleep wakeup, no rotary encoder (no PCNT), no touch. WiFi power reduction defaults on as a hardware workaround. |
| ESP8266 / ESP-12 | 4 MB. OTA works. | **Supported** | Tested in CI (nodemcuv2). Used by the iotsa board. No BLE. HTTPS not recommended, see below. |
| ESP32-S3 | 4 MB flash + 2 MB embedded PSRAM (SuperMini clone). OTA works with `min_spiffs.csv`. | **Supported** | Tested in CI (esp32s3supermini). BLE confirmed working, including BLE/HPS-only (no WiFi) operation. Native USB via USB-Serial/JTAG (`esp32s3jtag`); USB-OTG/TinyUSB not yet implemented. Cheap SuperMini clone boards can have WiFi-reliability issues (runs hot, drops WiFi intermittently) even with `wifiPowerReduction` enabled — not fully resolved, see #194. |
| ESP32-C6 | 4 MB. OTA likely works. | **Untested** | RISC-V like C3, adds WiFi 6 and Bluetooth 5.3. Likely similar to C3 — _feedback welcome_. |
| ESP32-S2 | 4 MB. OTA likely works. | **Untested** | No Bluetooth; BLE-dependent features will not work. _Feedback welcome on what does work._ |
| ESP32-H2 | — | **Not supported** | No WiFi. Not a viable target. |

## Installation and use for developers

Iotsa can be used with both the [Arduino IDE](https://www.arduino.cc/en/main/software) and with the [PlatformIO](https://platformio.org) build system (which can be used from within VSCode or from the command line). The Arduino IDE is easiest to get started with, but PlatformIO is more powerful if you want to target multiple device types, use _Git_ integration, etc.

### Arduino IDE

The normal way to install iotsa is through the Arduino Library Manager: _Sketch_ -> _Include Library_ -> _Manage Libraries..._, search for _iotsa_, and install it. This also takes care of iotsa's own dependencies.

If you need a version that hasn't been released yet, download the zipfile (via <https://github.com/cwi-dis/iotsa>) and install into Arduino IDE with _Sketch_ -> _Include Library_ -> _Add .ZIP Library..._ instead. (If you downloaded the zipfile through github you may have to rename the directory because it may be called something like _iotsa___master_).

Build the _Hello_ example (_File -> Examples -> iotsa -> Hello_) and flash it onto an ESP32-based board (or an ESP8266/ESP-12 board, such as the iotsa board -- see [Choosing hardware](#choosing-hardware) above).

**Note:** the structure of the iotsa examples may be slightly different than what you are used to for Arduino examples: the `Hello.ino` file is basically empty, and the code is contained in the `mainHello.cpp` file, which is in a separate tab in the Arduino IDE.

Using the `arduino-cli` tool is supported too.

### PlatformIO

The _iotsa_ library is known to the PlatformIO library manager, so simply adding it to your _platformio.ini_ file should do the trick for adding the iotsa framework to your project..

To build an example against your local (possibly uncommitted) changes to the iotsa library itself, open the `iotsa` source directory in _VSCode_ or _Atom_ and look for the `[env:HelloIotsa-example-iotsa_v4]` section (these `[env:...]` sections are generated from `examples/*/iotsa-build.json` and `tests/*/iotsa-build.json` into `generated_envs.ini` -- see `extras/python/gen_build_matrix.py`). Replace the references to _HelloIotsa_ with the name of the example you want to build.

But: each example in the _examples_ folder also has its own _platformio.ini_ file, generated the same way, for building it standalone (pulling iotsa from GitHub rather than your local checkout). So you can also open `iotsa/examples/Hello` in _VSCode_ or _Atom_ and build it, or use the command line:

```
$ cd iotsa/examples/Hello
$ platformio run --target build
$ platformio run --target upload
```

### Supporting a new board

There are three tiers of "a board works with iotsa", depending on how much investment
it deserves.

**1. One-off: does this one example build for this one board?** Fully throwaway --
touches nothing outside the example's own directory. Every example already has its own
generated `platformio.ini` (see above); if the one you need doesn't have one yet,
generate a starting point first:

```
$ python3 extras/python/gen_build_matrix.py --format=standalone-ini \
    --dir=examples/<Example> -o examples/<Example>/platformio.ini
```

Then hand-add a scratch `[env:...]` section to that file. It does **not** extend
anything from the toplevel `platformio.ini` -- each example's own `platformio.ini` is
self-contained -- so spell out `platform`, `board`, and any `build_flags` /
`board_build.partitions` directly:

```ini
[env:quicktest]
extends = common
platform = espressif32@7.1.3
board_build.filesystem = littlefs
board = esp32-c3-devkitm-1
build_flags = -DESP32C3 -DWITH_ROUNDLCD_DISPLAY
board_build.partitions = min_spiffs.csv
```

```
$ cd examples/<Example>
$ pio run -e quicktest
```

When done, discard the scratch section (`git checkout -- examples/<Example>/platformio.ini`,
or simply don't `git add` it). If the actual fix under test lived in `src/`, that is a
normal, separate change to commit -- only the throwaway env section needs undoing.

**2. First-class citizen: fully covered by CI.** For a board other iotsa apps are
expected to build against routinely, worth catching regressions on for every push:

- If this is a genuinely new chip family or USB wiring, hand-add a trait section to
  `iotsa-board-traits.ini` (leading underscore, e.g. `_esp32_allvariants`/`_esp32c3_extusb`/
  `_esp32c3_nativeusb`/`_esp8266`) and mirror its `build_flags` into
  `TRAIT_BUILD_FLAGS` in `extras/python/gen_build_matrix.py` -- see the layer-1/2/3
  board model, #222. Most new boards reuse an existing trait and skip this step.
- Add the board to `boards` in **`iotsa-board.json`** (the single source of truth):
  the build fields `pio_board`, `fqbn` (or `null`), `trait`, `family`
  (`esp8266`/`esp32`), `build_flags` (e.g. a neopixel pin) and any of
  `partitions`/`mcu`/`flash_size`; plus the human fields `display_name`,
  `description`, `chip`, `reference`, `provenance`, `best_example`, `constraints`
  (or `null`), `status`, `notes` -- these feed `docs/boards.md` and nothing else.
  **Easy to forget the build side, and forgetting it fails quietly:**
  `emit_platformio_ini`/`emit_github_matrix` just print a `warning: unknown board
  ... skipping` to stderr and drop the env, rather than erroring. (Only
  `emit_standalone_ini`, used in tiers 1 and 3, hard-fails with a `KeyError` on an
  unknown board -- if you hit that, this is the fix.)
- Add a variant entry for the board to `iotsa-build.json` in every example (and test)
  you want it covered for.
- Regenerate the generated files and commit them alongside the source changes:

```
$ python3 extras/python/gen_build_matrix.py --format=board-defs-ini -o iotsa-board-defs.ini
$ python3 extras/python/gen_build_matrix.py --format=boards-md -o docs/boards.md
$ python3 extras/python/gen_build_matrix.py --format=platformio-ini -o generated_envs.ini
$ python3 extras/python/gen_build_matrix.py --format=standalone-ini \
    --dir=examples/<Example> -o examples/<Example>/platformio.ini   # for each touched example
```

- Build the new env(s) to confirm before committing, e.g. `pio run -e <Example>-example-<boardname>`.

**3. Second-class citizen: defined, but not in CI.** For a board close enough to an
existing CI-covered one that it does not need its own automated build, but where future
iotsa *applications* will target it and should have a correct, already-verified board
definition to start from. Do the first two steps from tier 2 (the trait, if new, and
the `iotsa-board.json` entry), regenerate `iotsa-board-defs.ini` and `docs/boards.md`,
but skip the `iotsa-build.json` step -- that is what keeps it out of
`generated_envs.ini` and the CI matrix. Verify it once using tier 1's throwaway
single-example recipe, then commit the `iotsa-board.json` entry and the regenerated
files.

Worth knowing either way: `iotsa-board-traits.ini`/`iotsa-board-defs.ini` (pulled into the toplevel
`platformio.ini` via `extra_configs`) only govern building *iotsa's own* examples/tests
as a project. Downstream application repos (`iotsaNeoClock`, the `lissabon` apps, etc.)
never reference these sections -- they pull in iotsa only as a `lib_deps` library, and
each app repo defines its own board section(s) from scratch in its own `platformio.ini`
(see e.g. `iotsaNeoClock`'s `[esp32c3]`/`[env:crowpanel128]`). So a tier-2 or tier-3
board here is not automatically "used" by an app; it exists as the canonical, working
reference an app author copies pin/flag choices from, and as what keeps iotsa's own
example/test builds honest for that board. (Sharing `iotsa-board-traits.ini`/`iotsa-board-defs.ini`
with those repos directly, instead of copying choices out of them by hand, is still an
open idea -- see #222.)

**Careful with `pio pkg install`/`pio pkg update -l <library>`** (in iotsa and in
downstream repos alike). Unless you add `--no-save`, PlatformIO saves the library into
the env's own `lib_deps` and rewrites the whole `platformio.ini`. With this layout,
where envs inherit `lib_deps` (and much else) through `extends` from shared sections,
that new env-level `lib_deps` *replaces* the inherited list: other libraries (e.g.
Adafruit GFX, NeoPixelBus in lissabon) silently disappear from the build. To get the
newest iotsa `develop` into a project, delete `.pio/libdeps/<env>/iotsa` and let the next
`pio run` fetch it, or use `--no-save`. After any `pio pkg` command, check `git status`
for modified `platformio.ini` files.

### Both Arduino IDE and PlatformIO

On reboot, the board will first initialize the LittleFS flash filesystem (if needed) and then create a WiFi network with a name similar to _config-iotsa1234_. Connect a device to that network and visit <http://192.168.4.1>. Configure your device name (at <http://192.168.4.1/config>) and WiFi name and password (at <http://192.168.4.1/wificonfig>), and after reboot the iotsa board should connect to your network and be visible as <http://yourdevicename.local>.

When the device is running normally you can visit <http://yourdevicename.local/runmode> and request the device to go into configuration mode, or to do a factory reset. After requesting this you have a few minutes to power cycle the device to make it go into configuration mode again (see previous paragraph) or do a complete factory reset. When in configuration mode you have a few minutes to change the configuration (device name, WiFi name, password, maybe other parameters) before the device reverts to normal operation. The idea behind this sequence (_request configuration mode_, then _power cycle_, then _change parameters_) is that you need both network acccess and physical access before you can do a disruptive operation on the device.

### Build time options

A number of features of the iotsa framework can be enabled and disabled selectively at build time. These features are encoded in `iotsaBuildOptions.h`. 

The general naming convention is that a feature _WIFI_ will be triggered by a define `IOTSA_WITH_WIFI`. If the _WIFI_ feature is enabled by default (which happens to be the case for _WIFI_, obviously) that can be overridden by defining `IOTSA_WITHOUT_WIFI` at compile time of the iotsa library.

When using the Arduino IDE you can edit this file to change the options, by commenting out the defines for features you do not need.

When using Platformio you can also use the `build_flags` in _platformio.ini_. For example, to enable COAP and disable REST:

```
	build_flags = -DIOTSA_WITHOUT_REST -DIOTSA_WITH_COAP
```

The following features are defined:

- `DEBUG` The debug log lines (`IOTSA_LOG_DEBUG`, see [iotsaLog.h](#iotsalogh)). Default on. A device can also switch them off at runtime.
- `WIFI` The WiFi network interface. Default on.
- `OTA` Over-the-air updates. Default on when `WIFI` is.
- `HTTP` The web server. Default on.
- `HTTPS` The secure web server. Default off. If `HTTP` is on as well, plain http requests are redirected to https.
- `WEB` The web pages for people. Default on. Requires `HTTP` or `HTTPS`.
- `REST` The REST API over http(s). Default on. Requires `HTTP` or `HTTPS`.
- `COAP` The CoAP API over UDP. Default off. Known not to work in 3.0 ([#215](https://github.com/cwi-dis/iotsa/issues/215)).
- `BLE` Bluetooth LE, ESP32 only: the BLE server, and the API over BLE (HPS; `IOTSA_WITHOUT_HPS` leaves that out). Default off.
- `SLEEP` Sleep and wake. Default on when `BLE` is, off otherwise.

There is no `API` option any more: the API handlers are always compiled, and the transports above decide how they can be reached. `IOTSA_WITHOUT_STATUS_LED` leaves out the status LED on a board that has one. The debug subsystems (`IOTSA_DEBUG_WIFI` and so on) are listed in `iotsaLog.h`. There are a few more options, see `iotsaBuildOptions.h`.

## HTTPS support

If a iotsa application is built with `IOTSA_WITH_HTTPS` if will initially use a builtin key and certificate. This is **NOT** secure, because the key and certificate are contained in the github source repository, and therefore known to the world.

After building and flashing your software for the first time you should create a new, unique, key and certificate. There are three ways to do this, using scripts in the `extras` directory:

- `extras/make-self-signed-cert.sh` creates a self-signed certificate. This can be copied into your source code (before building and flashing).
- `extras/make-csr-step{1,2,3}.sh` these create a key and self-signed or CA-signed certificate that can be uploaded to the iotsa device using `iotsa`.
- `extras/make-igor-signed-cert.sh` creates a key and certificate signed by your [Igor](https://github.com/cwi-dis/igor) CA and uploads it to your device.

Note that HTTPS support here refers to iotsa as a server only, HTTPS client support (for the _iotsaButton_ and _iotsaRequest_ modules) is completely independent.

**HTTPS on ESP8266 is not recommended.** BearSSL on that chip's limited RAM/CPU has proven unreliable in practice: ordinary API requests can take up to a minute, and the device can crash under the memory/CPU pressure (see [#159](https://github.com/cwi-dis/iotsa/issues/159)). Building with `IOTSA_WITH_HTTPS` on ESP8266 now emits a compile-time `#warning`. Prefer HTTP-only on ESP8266; HTTPS remains solid on ESP32.

## Controlling iotsa devices

### Command line tool

There is a command line helper program `iotsa`. See [extras/python/README.md](extras/python/README.md) for full documentation and usage examples.

To install from the repo root:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -e extras/python/
```

(If instructions in this repo's parent directory cover venv setup, those take precedence over the steps above.)

Then use `iotsa --help` for a list of all options and commands.

Whenever you made significant changes to your source tree (and committed them) you may want to rerun `pip install -e extras/python` to refresh the version string.


### OTA programming

If you have enabled over-the-air programming <http://yourdevicename.local/runmode> will also allow you to request the device to go into programmable mode. Again, you have a few minutes to power cycle and then a few minutes to reprogram:

- _For Arduino IDE_: 
  - In _Tools_ -> _Port_ -> _Network Port_ select your device.
    - Sometimes ota-programmable devices are slow to appear because of mDNS issues. On a Mac, run the following command in a _Terminal_ window to speed up discovery:

      ```
      dns-sd -B _services._dns-sd._udp.
      ```

      or, on Linux,

      ```
      avahi-browse _services._dns-sd._udp
      ```

  - Use the normal _Upload_ command to flash your new program.
- _For PlatformIO_: 
  - Visit <http://yourdevicename.local> and note the IP address.
  - Edit `platform.ini` and add the `upload_port` setting with the correct host name or IP address,
  - or upload with the following command line:
  ```
  platformio run -t upload --upload-port yourdevicename.local
  ```

- _For PlatformIO, using iotsa_:
	- Build using `platformio run` or the platformio IDE integration commands.
	- `iotsa --target yourdevicename.local otaWait ota ./.pio/build/nodemcuv2/firmware.bin`
	- Power cycle the device when _iotsa_ asks you to do so.
	
## General design philosophy

Unlike most Arduino libraries and frameworks Iotsa does expose some of its C++ interfaces, but for simple applications you do not have to worry about this.

Iotsa has two main types of objects:

* `IotsaApplication`, of which there is only one: the container for the modules.
* `IotsaModule`, of which there can be many. A module provides some functionality, plus a way for the outside world to see and change it: a web page, a REST (or CoAP) API, sometimes a BLE service. You subclass `IotsaModule` for your own functionality.

  The standard modules for configuration, the run mode, WiFi, over-the-air updates and the web server (and the BLE server, when BLE is built in) are created automatically, see [module categories](#module-categories). Your program only declares its own modules, plus any optional standard modules it wants (see [available standard modules](#available-standard-modules)).

  For the very simplest case there is `IotsaSimpleMod`, which turns two functions into a module.

The rest of this section follows the `Hello` examples. The [sample programs](#sample-programs) section lists all examples.

### Do-nothing application

You create a global variable `application` of type `IotsaApplication`. The WiFi module (so the end user can configure the WiFi network to join and such) and the OTA module are created automatically:

```
IotsaApplication application("Iotsa Hello World Server");
```

In your `setup()` function you call `application.setup()` and `application.lateSetup()`, which initialize the framework and all modules:

```
void setup(void){
  application.setup();
  application.lateSetup();
  // Add your own setup code here....
}
```

In your `loop()` function you call `application.loop()`. This handles requests and everything else the framework needs:

```
void loop(void){
  application.loop();
  // Add your own loop code here....
}
```

You have now created a Iotsa server that does absolutely nothing.

For a real application, start from [HelloIotsa](examples/HelloIotsa/HelloIotsa.ino) instead: it is structured the way we recommend, with your module in its own `.h` and `.cpp` file.

### Hello world application

One step more functionality is provided by the [Hello](examples/Hello/Hello.ino) example, a single file in Arduino-sketch style. It has a handler function for the `/hello` page (with a form through which a user can type in their name) and an info function that returns a paragraph for the home page `/`:

```
void helloHandler() {
  if (application.server->hasArg("personName")) {
    personName = application.server->arg("personName");
  }
  String message = "<html>...construct html page...</html>";
  application.server->send(200, "text/html", message);
}

String helloInfo() {
  return "<p>See <a href=\"/hello\">/hello</a> to change the name.</p>";
}
```

These two functions (and the URL) are registered in the application as a `IotsaSimpleMod`:

```
IotsaSimpleMod helloMod(application, "/hello", helloHandler, helloInfo);
```

### Hello world as a module

From a functionality point of view [HelloIotsa](examples/HelloIotsa) is _Hello_, plus a REST API and keeping the name over reboots. But it is structured as a C++ class, the way we recommend for your own applications: the module class lives in its own `.h`/`.cpp` pair (`iotsaHelloMod.h`/`iotsaHelloMod.cpp`), and the main file only contains the application glue (which modules to create, plus `setup()`/`loop()`). The module can then be reused in other devices, and most standard modules started their life this way.

You declare the class for your functionality:

```
class IotsaHelloMod : public IotsaModule {
public:
  using IotsaModule::IotsaModule;
  void setup() override;
  void lateSetup() override;
  void loop() override;
  String info() override;
protected:
  void configLoad() override;
  void configSave() override;
  void putHandler(IotsaApiContext& ctx) override;
  void getHandler(IotsaApiContext& ctx) override;
  String webTitle() override { return "Hello Server"; }
  void formHandler_args(IotsaWebContext& ctx) override;
  void formHandler_fields(IotsaWebContext& ctx) override;
private:
  String personName;
};
```

In `lateSetup()` one call registers both the REST API and the web page: GET and PUT on `/api/hello`, and the page `/hello`:

```
void IotsaHelloMod::lateSetup() {
  api.setup("hello", true, true);
  name = "hello";
}
```

Finally the main file creates a single object of your module:

```
IotsaHelloMod helloMod(application);
```

Iotsa now calls your module's `setup()`, `lateSetup()` and `loop()` at the right time, without any code in the normal `setup()` and `loop()` functions. So this module can be combined with as many other modules as you want, just by adding that one-line declaration.

### The API

The REST (or CoAP, or BLE/HPS) API is two methods. `putHandler()` picks up what the caller sent; `getHandler()` puts the values into the reply:

```
void IotsaHelloMod::putHandler(IotsaApiContext& ctx) {
  ctx.arg("personName", personName, IotsaApiContext::Saved);
}

void IotsaHelloMod::getHandler(IotsaApiContext& ctx) {
  ctx.show("personName", personName);
}
```

The `ctx` object handles the common things: `ctx.arg()` checks the caller's rights and the type of the field, and because `personName` is `Saved` the framework calls `configSave()` when it changed. Fields nobody read are reported back to the caller as `iotsa_api_ignored`. To refuse, call `ctx.error(status, "why")`; the first error stops the rest of the request. Do anything that has an effect (start a flash, ring) after reading all fields, behind `if (ctx.failed()) return;`.

### The web page

The page is two methods too, and the framework does the rest (the page itself, feedback, authentication, saving the configuration). `formHandler_args()` picks up what the form sent, like `putHandler()`; `formHandler_fields()` writes the page body, like `getHandler()`:

```
void IotsaHelloMod::formHandler_args(IotsaWebContext& ctx) {
  ctx.arg("personName", personName, IotsaWebContext::Saved);
}

void IotsaHelloMod::formHandler_fields(IotsaWebContext& ctx) {
  if (!ctx.includesEveryday()) return;
  ctx << "<p>Hello, " + htmlEncode(personName) + "!</p>\n";
  IotsaWebContext::Form form(ctx, "Set");
  ctx.showField("Name", "personName", personName, IotsaWebContext::Saved);
}
```

`arg*()` reads an argument from the request, `show*()` writes to the page. Every call says the field's persistence, what a change means: `Transient` is state that isn't saved (an LED, an alarm), `Saved` is saved (the framework calls `configSave()`), `SavedInConfigMode` is saved and can only be changed in configuration mode (host name, WiFi network). A page has an everyday part and a settings part; `formHandler_fields()` is called once for each (`ctx.includesEveryday()`, `ctx.includesSettings()`).

`ctx.showField()`, `ctx.showStatus()` and friends encode their values for HTML themselves. When you write HTML yourself, with `ctx <<` or in `info()`, encode interpolated strings with `htmlEncode()`.

### Triggers

A web page is for people. For another device, or `curl`, a module can add a trigger: a plain URL that does something right away, with its arguments in the query. The [Ringer](examples/Ringer/Ringer.ino) example rings on `/ring?alarm=20`:

```
addTrigger("ring", [this](IotsaWebContext& ctx) { ringTrigger(ctx); });
```

### Next steps

[HelloPasswd](examples/HelloPasswd/HelloPasswd.ino), [HelloUser](examples/HelloUser/HelloUser.ino) and [HelloToken](examples/HelloToken/HelloToken.ino) each take the `HelloIotsa` module and add one access-control mechanism, gating both the web form and the REST API.

For hardware, start with [Light](examples/Light/Light.ino), a module that only measures (a GET and a page without a form), and [Blink](examples/Blink/Blink.ino), a module that drives an output from a setting. Then see the [sample programs](#sample-programs) below.

## Core API

### iotsa.h

This file declares the `IotsaApplication` class, and `IotsaBaseModule`, the base class of all modules. `IotsaModule` (in `iotsaApi.h`) adds the API and the web page. `IotsaAuthMod` is a module that can be the application's _authenticator_: it gives the modules access control, so they only work after the user has provided a username/password combination, a capability token, or any other means of authentication you can think of.

The constructors:

```
IotsaApplication(const char *_title);
IotsaModule(IotsaApplication &_app, bool early=false);
IotsaAuthMod(IotsaApplication &_app, bool early=false);
```

An application has a single authenticator, used by every module, the standard ones (WiFi, OTA, configuration, runmode) included. Set it in `setup()`, before `application.setup()`:

```
IotsaUserMod myAuthenticator(application);
...
void setup() {
  application.setAuth(&myAuthenticator);
  application.setup();
  ...
}
```

With a stack of authenticators (e.g. `IotsaCapabilityMod` on top of `IotsaUserMod`, although capabilities don't work in 3.0, see [iotsaCapabilities.h](#iotsacapabilitiesh)) pass the top one. Without `setAuth()` everything is allowed.

The optional `early` argument makes the module set up before the others. Standard modules that the others depend on use it (configuration, run mode, WiFi, the web server, the logger, the status LED, battery); an application module normally doesn't need it.

`application.setup()` mounts the LittleFS filesystem first, and formats it if it can't be mounted.

### iotsaApi.h

Contains `IotsaModule`, the base class for a module with an API and a web page. Register your endpoint in `lateSetup()`:

```
api.setup(const char* path, bool get=false, bool put=false, bool post=false, bool webPage=true);
```

This registers `/api/<path>` for REST (and CoAP, and BLE/HPS when built in), and, when `get` and `webPage` are true, the web page `/<path>`. Call it several times if your module has several endpoints (`ctx.path()` says which). Then implement the handlers for the methods you passed `true` for:

```
void getHandler(IotsaApiContext& ctx);
void putHandler(IotsaApiContext& ctx);
void postHandler(IotsaApiContext& ctx);
```

The put side reads fields with `ctx.arg()` (and `ctx.argPassword()` for secrets), the get side writes them with `ctx.show()`. Changing and reading a field is open to everyone on the network, unless a right is named: `ctx.argsNeedRight("name")` for changes, `ctx.showsNeedRight("name")` for reads, or once for the whole module with `changesNeedRight()`/`readsNeedRight()` in `lateSetup()`. A field the caller may not read is listed in `iotsa_api_redacted`, or shown as "(hidden)" on a web page; without credentials, a web page asks for them. A write-only value, such as a password, is `ctx.showsNeedRight(IOTSA_RIGHT_NEVER)`. Objects inside a module (users, buttons) get the same handlers, called inside an `IotsaApiContext::Scope`. See `iotsaContext.h` and the Decided comment on cwi-dis/iotsa#296.

A list of objects with Add and Remove (users, tokens) is a collection: subclass `IotsaVector<T>` around the `std::vector<T>`, and call its handlers from the module's. It gives the page a table or a section per element, and the API one shape under the collection's name ([#309](https://github.com/cwi-dis/iotsa/issues/309)): a GET returns `"users": [...]`; a PUT of an array replaces the whole list, so a restore sends back what a backup read; a PUT of an object keyed by name (or index) changes or adds elements, `null` removes one; a POST of `"tokens": {...}` adds one. This is iotsa's own rule, borrowed from JSON Merge Patch but not that. See `iotsaCollection.h`.

The web page is `formHandler_args()` and `formHandler_fields()`, see [The web page](#the-web-page) above and `iotsaWebContext.h`. A trigger is `addTrigger()`, see [Triggers](#triggers).

### iotsaConfig.h

The `iotsaConfig` object holds the device's own settings: the hostname, how long a maintenance mode lasts, and the HTTPS certificate. It doesn't hold the mode the device is in; that is `iotsaController`'s. You normally don't include this file, it is included through `iotsa.h`.

### iotsaController.h and iotsaStatus.h

`iotsaController` decides the device's mode (normal, configuration, OTA, factory reset), when it may sleep, which radios are on, and when it reboots. What an application module typically calls:

```
iotsaController.inConfigurationMode();
iotsaController.extendCurrentMode();               // there was activity, keep the mode open
iotsaController.allowRequestedConfigurationMode(); // the user confirmed a requested mode, e.g. with button taps
iotsaController.allowRCMDescription("tap the button 3 times"); // how, for the web page
iotsaController.postponeSleep(ms);
iotsaController.pauseSleep(); iotsaController.resumeSleep();
iotsaController.requestReboot(ms);
```

`iotsaStatus` is the board where the controllers publish the device's state, for reading: `networkIsUp()`, `onUsbPower`, `statusColor()` (what the status LED shows). `iotsaStatus.setStatusPulse(rgb, onMs, offMs, durationMs, reason)` shows a pulse of your own on the status LED, see [iotsaLed.h](#iotsaledh). See [docs/controller-architecture.md](docs/controller-architecture.md) for the design.

### iotsaLog.h

Log output, through three macros, included by `iotsa.h`. Each writes one line `tag: ...` to the serial port (and to the [logger](#iotsaloggerh) if there is one):

```
IOTSA_LOG("myApp", "door opened by %s", name);  // always: state changes, errors, refused requests
IOTSA_LOG_DEBUG("myApp", "step %d", n);         // debug: on by default, switchable per device
IOTSA_LOG_DEBUG_WIFI("iotsaWifi", "...");       // a subsystem: only with -DIOTSA_DEBUG_WIFI
```

The debug lines are switched at runtime by `debugLog` on `/runmode` and `/api/runmode`, and left out of the build with `-DIOTSA_WITHOUT_DEBUG`. `tag` and the format must be string literals, and need no trailing newline. Every API request gets one line with its result, from iotsa itself. The subsystems are listed in `iotsaLog.h`. `IotsaSerial` is still there for output that isn't a log line.

### iotsaConfigMod.h

The configuration module: `/config` and `/api/config`, for the hostname, how long a maintenance mode lasts, whether WiFi and BLE are off at boot, and the HTTPS certificate. Most of these can only be changed in configuration mode. `/api/version` says which program and iotsa version the device runs. Created automatically, also without WiFi; don't declare it.

`/api/config` still accepts the mode and reboot fields that moved to `/api/runmode`, for older scripts.

### iotsaRunmode.h

The run mode module: `/runmode` and `/api/runmode`. Created automatically; don't declare it.

A user can request configuration mode, OTA mode or a factory reset here. The user must then _demonstrate physical access_ within a few minutes to switch the device to its new mode, which lasts for a few minutes (the configuration module's timeout), longer while it is being used. If nothing happens, the device stays in normal mode.

> The intention of _demonstrating physical access_ is that any "dangerous" operation requires both network access (to request the operation) and physical access. The user can always turn power off and on to demonstrate physical access. A program can allow its own way (`iotsaController.allowRequestedConfigurationMode()`, for example after a number of quick button presses), and over BLE a confirmation is allowed unless the BLE server's `promoteAllowed` setting is off.

On a factory reset all configuration information (literally: _all_) is forgotten and the iotsa device is completely new again.

In OTA mode the device works normally, but can also be reprogrammed over the network, see [OTA programming](#ota-programming).

The module can also reboot the device, switch WiFi and BLE off and on at runtime, and switch the debug log lines (`debugLog`). With sleep built in (`SLEEP`) it holds the sleep settings (`sleepMode`, `sleepDuration`, `wakeDuration` and more); `setPinDisableSleep(pin)` names a pin that, held low, keeps the device awake. `/api/status` (read only) has the mode, the WiFi state, the boot cause, the uptime and how full the filesystem is.

### iotsaWifi.h

Handles WiFi configuration.
Created automatically whenever WiFi is compiled in; you don't need to declare it. Building without WiFi (`-DIOTSA_WITHOUT_WIFI`) is possible, but except for special use cases such as creating a Bluetooth LE only service it is not recommended.

A iotsa device can join an existing WiFi network (normal WiFi mode) or create a temporary network as an Access Point (private WiFi mode). 
In private mode the device does not connect to a WiFi network, but in stead creates its own network (as a base station) with a name starting with "_config-_". The user can now connect a device to this network and visit `http://192.168.4.1/wificonfig`. Here it is possible to set the normal WiFi network to connect to and the password.

If a device is new (it has no WiFi network name) it will enter private mode automatically. If a device can't reach its configured network for 10 seconds, it alternates: 10 seconds looking for the network, then 30 seconds of private mode, so a user can join and fix the settings. While someone is connected to the private network, it doesn't go looking. As soon as it joins its network it is back in normal mode.

If a device is in normal WiFi mode the WiFi parameters can only be changed if the device is in configuration mode.

The module also provides a REST api on `/api/wificonfig` (and this api depends on whether in configuration mode or not).

### iotsaSimple.h

Allows implementing a module using two simple C functions. See the _hello_ section above for details.


### iotsaConfigFile.h

Two classes to save and load configuration variables to a file (on the LittleFS file system in the flash memory chip). `IotsaConfigFileLoad` is used to load configuration values and `IotsaConfigFileSave` to store them. 

The general paradigm is that in your `setup()` function you create a local (stack based) loader variable with 

```
IotsaConfigFileLoad cf("/config/filename.cfg");
```
and then repeatedly call `cf.get("name", variable, default)` for each configuration variable. Variables can be of type `int` (and the unsigned types), `bool`, `float`, `String` or `std::string`.

To save configuration values you call

```
IotsaConfigFileSave cf("/config/filename.cfg");
```

and then repeatedly call `cf.put("name", variable)`.


## available standard modules

### Module categories

How a module gets into your program, and what it is for. For how the parts fit together inside iotsa, see [docs/architecture.md](docs/architecture.md).

**Automatic modules.** `application.setup()` creates these itself, unless your program declares one (to pass arguments, for instance):

- configuration (`iotsaConfigMod.h`) and run mode (`iotsaRunmode.h`): always.
- WiFi (`iotsaWifi.h`): unless `-DIOTSA_WITHOUT_WIFI`.
- OTA (`iotsaOta.h`): when WiFi is in, unless `-DIOTSA_WITHOUT_OTA`.
- the web server (`iotsaHttpServer.h`): unless `-DIOTSA_WITHOUT_HTTP` and no `-DIOTSA_WITH_HTTPS`. It serves the web pages and the REST API.
- the CoAP server: with `-DIOTSA_WITH_COAP`.
- the BLE server (`iotsaBLEServer.h`) and the API over BLE (HPS): with `-DIOTSA_WITH_BLE`.
- the status LED (`iotsaLed.h`): when the board definition has one, unless `-DIOTSA_WITHOUT_STATUS_LED`.

**Optional standard modules.** Declare them in your program, see [Adding a standard module](#adding-a-standard-module):

- access control: `iotsaUser.h`, `iotsaCapabilities.h` (doesn't work in 3.0).
- time: `iotsaNtp.h`, `iotsaRtc.h`.
- power: `iotsaBattery.h` (sleep itself is the run mode module's).
- maintenance: `iotsaLogger.h`, `iotsaFiles.h`, `iotsaFilesUpload.h`, `iotsaFilesBackup.h`.
- input hardware: `iotsaInput.h`, and `iotsaButton.h` (deprecated).
- this device as a BLE client: `iotsaBLEClient.h`, `iotsaBLEClientManager.h`.
- for development: `iotsaSimple.h` (a module from two functions), `iotsaNothing.h` (a test fixture).

**Building blocks.** Not modules, but classes your own module uses: `iotsaApi.h` and `iotsaContext.h` (the API and its handlers), `iotsaWebContext.h` (the web page), `iotsaCollection.h` (a list of objects), `iotsaConfigFile.h` (saving settings), `iotsaLog.h` (log lines), `iotsaDeadline.h` (timers), `iotsaRequest.h` (sending http(s) requests).

`iotsaConfigMod.h`, `iotsaRunmode.h`, `iotsaWifi.h`, `iotsaSimple.h` and the building blocks are described in the [Core API](#core-api) section above (as far as they are described at all); the other modules alphabetically below.

### Adding a standard module

To add an optional standard module, include its header and declare it next to your application, in the main file:

```
#include "iotsaNtp.h"
IotsaNtpMod ntpMod(application);
```

That's all: its web page and API appear by themselves. Some modules need more (pins, or `setAuth()` for an authenticator); that is described with the module below.

### iotsaBattery.h

Measures the battery and USB voltages (`setPinVBat()`, `setPinVUSB()`), shows them on `/battery` and `/api/battery`, and offers the standard BLE battery service. Whether the device is on USB power goes to `iotsaStatus.onUsbPower`, which the sleep settings can use. Sleep itself is the [run mode module](#iotsarunmodeh)'s. See the [LedBattery](examples/LedBattery/LedBattery.ino) example.

### iotsaBLEServer.h

The device as a Bluetooth LE peripheral. ESP32 only, with `-DIOTSA_WITH_BLE`; created automatically then. Every module's API is reachable over BLE through HPS. A module can also have a BLE service of its own: an `IotsaBleApiService` member, its characteristics added in `setup()`, and `bleGetHandler()`/`blePutHandler()` (from `IotsaBLEProvider`, which every module has) to answer them. See the [Led](examples/Led) example.

`/bleserver` and `/api/bleserver` hold its settings: on or off (`isEnabled`), the advertising interval (`adv_min`, `adv_max`), the transmit power (`tx_power_dbm`), how long an idle connection stays (`idle_timeout`), and whether a mode switch may be confirmed over BLE (`promoteAllowed`, default on).

BLE takes a lot of flash, about 160KB; see _Flash size and OTA_ under [Choosing hardware](#choosing-hardware).

### iotsaBLEClient.h

The device as a Bluetooth LE central: it scans, and connects to other BLE devices to read and write their characteristics, one connection at a time. `/bleclient` and `/api/bleclient` hold the scan settings. A program names the devices it talks to itself. See the [BLEButton](examples/BLEButton) example. There is no generic "send a command to that device" yet, like `iotsaRequest.h` has for http ([#208](https://github.com/cwi-dis/iotsa/issues/208)).

### iotsaBLEClientManager.h

`iotsaBLEClient.h`, plus a list of known devices that users can change: on the page and the API they can add, rename and remove devices, and see which unknown devices are nearby. The devices are saved. Subclass it for a device that manages others, see the [BLEManager](examples/BLEManager) example.

### iotsaButton.h

A generalized module for handling buttons and switches attached to GPIO pins. Uses _iotsaRequest.h_ to call a URL when a button is pressed. See the [Button](examples/Button/Button.ino) example. Deprecated: will be merged into _iotsaInput.h_ and removed (#160).

### iotsaCapabilities.h

An authentication module (_IotsaAuthMod_) that trusts an external server, 
the _issuer_, to determine access rights to this board.

The _issuer_ and this board share a secret key.

**Does not work in 3.0:** every token is rejected (#304), and since the rights redesign a token could not grant a named right anyway. Capabilities come back with the new permission model (#107).

### iotsaFiles.h

Allows read access to files stored in `/data` on the LittleFS file system (in the flash memory chip). Could be used for a simple web server. Requires _IOTSA\_WITH\_WEB_.

### iotsaFilesUpload.h

Allows write access to files stored in `/data` on the LittleFS filesystem (where _iotsaFiles_ reads from), through `POST` requests to the `/upload` URL. Requires _IOTSA\_WITH\_WEB_.

### iotsaFilesBackup.h

Creates a backup of the complete LittleFS filesystem (including `/data` and `/config`) as a tarfile when you access URL `/backup.tar`. Can be used to clone iotsa devices. Requires _IOTSA\_WITH\_WEB_.

**Careful:** the backup includes the WiFi password, the users' passwords and other secrets, in plain text: without them it couldn't restore a device. It can only be downloaded in configuration mode, by anyone with the right `backupfiles` (anyone at all, without an authenticator) ([#300](https://github.com/cwi-dis/iotsa/issues/300)).

### iotsaInput.h

Handles buttons, rotary encoders and (on ESP32) capacitive touchpads for input. It can change your variables directly (`bindVar()`), call a callback on changes, and (on ESP32) wake the device from sleep. See the [Input](examples/Input/Input.ino) example. _iotsaButton.h_ will be merged into it (#160).

### iotsaLed.h

The status LED: a single NeoPixel, or a plain LED, driven by polling `iotsaStatus.statusColor()` every `loop()` call, so it shows status information (breathing/blinking, per subsystem) during the boot sequence and whenever the iotsa board is running in a nonstandard mode (configuration mode, OTA mode, etc). To show your own color/pattern from your program, call `iotsaStatus.setStatusPulse(rgb, onDuration, offDuration, durationMs, reason)` -- a transient pulse that decays back to the normal status display on its own once `durationMs` elapses, no restore logic needed.

A plain LED shows the rhythms and pulses as brightness; only the colour is lost.

The module is created automatically when the board definition describes a status LED: `IOTSA_PIN_LED=<pin>`, plus `IOTSA_LED_NEOPIXEL` for a NeoPixel, or `IOTSA_LED_ACTIVE_LOW` for a plain LED that lights when the pin is low (see [docs/boards.md](docs/boards.md) for which boards have one). Build with `-DIOTSA_WITHOUT_STATUS_LED` if your program uses that pin for something else. You can still declare it yourself, e.g. for a NeoPixel on another pin: `IotsaLedMod ledMod(application, pin)`; that one is then used instead.

The module does not provide a user-visible endpoint or REST api, but can be used as a base class for this. See [examples/Led](examples/Led) for an example of triggering a pulse over the web/REST/BLE API.

### iotsaLogger.h

Keeps the log output in a memory buffer too, and shows it at the URL `/logger`. The buffer is small, 4KB (`IOTSA_LOGGER_BUFFER_SIZE`), but this allows some debugging over WiFi. Requires _IOTSA\_WITH\_HTTP_ or _IOTSA\_WITH\_HTTPS_.

To use it, add `IotsaLoggerMod loggerMod(application);` next to your application. The [log macros](#iotsalogh) and `IotsaSerial` go to the serial port and into the buffer, `Serial` only to the serial port.

### iotsaNothing.h

A test fixture: a module that does nothing, with one setting, a web page at `/nothing` and a REST interface on `/api/nothing`. The sandbox and the tests use it as a trivial application module. For your own modules, start from [HelloIotsa](examples/HelloIotsa) instead.

### iotsaNtp.h

A module that contacts an NTP server to set the local clock. Accessing URL `/ntpconfig` allows setting the NTP server to use, as well as the timezone.

The modules sets the `libc` time, so functions like `time()` and `localtime()` will work with the correct time.

Your own module can ask it for the time:

```
unsigned long utcTime();  // Seconds since 1-Jan-1970, UTC (unix time)
int localSeconds();       // Local time-of-day, seconds (0-59)
int localMinutes();       // Local time-of-day, minutes (0-59)
int localHours();         // Local time-of-day, hours (0-23)
int localHours12();       // Local time-of-day, hours (0-11)
bool localIsPM();         // AM/PM indicator
String isoTime();         // Local time, "YYYY-MM-DDTHH:MM:SS"
```

Provides a user interface at `/ntpconfig` and a REST interface at `/api/ntpconfig`. Add it with `IotsaNtpMod ntpMod(application);`.

### iotsaRtc.h

Support for _DS1302_ realtime clock chip, which usually comes on a board with battery backup so it can run for years. Will set _libc_ clock from RTC shortly after boot, and periodically set RTC clock from the system clock. So, when used together with `iotsaNtp` this automatically uses the RTC time when no network is available, and resynchronizes the RTC when a network is available.

The RTC keeps UTC, and `isoTime()` (also `/api/rtcconfig`) returns it as `YYYY-MM-DDTHH:MM:SSZ`. For local time use `iotsaNtp`, which owns the timezone. Declaration order relative to `iotsaNtp` doesn't matter.

Add it with the chip's three pins: `IotsaRtcMod rtcMod(application, PIN_ENA, PIN_CLK, PIN_DAT);`.

### iotsaOta.h

Allows over-the-air reprogramming of a iotsa server. Created automatically whenever WiFi is compiled in; build with `-DIOTSA_WITHOUT_OTA` to leave it out (saves flash). In OTA mode (requested through the [run mode module](#iotsarunmodeh)) the device accepts a new program for a few minutes, from the `iotsa` command line tool (see [OTA programming](#ota-programming)) or from the Arduino IDE, where it shows up under menu _Tools_ -> _Port_, _Network Ports_.

### iotsaRequest.h

Allows issuing of _http_ and _https_ requests. Carrying username/password credentials and tokens is possible. This is not a complete module in itself, but provides the building blocks to allow other modules to easily add the ability to send (configurable) web requests.

### iotsaUser.h

An authentication module (_IotsaAuthMod_) that stores usernames, passwords and
rights. Other modules can then specify they are only accessible after the user
authenticates with a username/password combination that has the correct _right_ in
its set of rights.

Accessing URL `/users` allows adding, changing and removing users, in configuration
mode. A factory-installed device has no users, and allows all rights always.
Over the API the users are a collection (see above): `PUT /api/users` with
`{"users": {"jack": {"password": "...", "rights": "*"}}}` adds or changes jack,
`{"users": {"jack": null}}` removes that user. Changing a password needs the old one in
`old_password`. A restore of a backup keeps the passwords, which a backup doesn't have.

Add it as the application's authenticator: declare `IotsaUserMod userMod(application);`, and call `application.setAuth(&userMod)` in `setup()`. See the [HelloUser](examples/HelloUser/HelloUser.ino) example.


## sample programs

To start your own application, copy _HelloIotsa_ (below): it is structured the way we recommend.

The examples come in two strands. Every example after _HelloIotsa_ assumes you have read it, and its comments only explain what is new.

### The interface strand: the Hello family

How a module talks to the outside world. Each example builds on the previous one:

- [Hello](examples/Hello/Hello.ino) is the toe-in-the-water entry point for people coming
  from Arduino: the simplest possible "Hello, user" server, a single file, no other
  structure.
- [HelloIotsa](examples/HelloIotsa) is the minimal *reasonable normal* iotsa app: the same
  functionality as `Hello`, but implemented as a C++ class with a REST interface (GET/PUT)
  for changing the name, keeping the name over reboots, and structured the way we
  recommend for real applications (app module in its own `.h`/`.cpp`, separate from the
  application glue).
- The next three each take the `HelloIotsa` module unchanged and add exactly **one**
  access-control mechanism on top, gating the REST API and changes through the web form
  (looking at the page needs no login):
  - [HelloPasswd](examples/HelloPasswd/HelloPasswd.ino) — a hardcoded username and password
    ("admin", "admin"), in its own small `IotsaAuthMod` subclass.
  - [HelloUser](examples/HelloUser/HelloUser.ino) — users with rights, managed on the device
    (_IotsaUserMod_): changing the name needs the right "hello".
  - [HelloToken](examples/HelloToken/HelloToken.ino) — a user login or a static API token
    with specific rights (_IotsaUserMod_ + _IotsaStaticTokenMod_).

  All three build with HTTPS by default (when using PlatformIO), so passwords and tokens
  don't travel in the clear.

### The device strand

Modules that read or drive hardware, and devices that talk to each other:

- [Light](examples/Light/Light.ino) measures the ambient light level with an LDR connected to the analog input. The smallest input module: read-only.
- [Blink](examples/Blink/Blink.ino) is Arduino's Blink the iotsa way: no `delay()`, and the interval can be changed on a web page or over REST (not saved). The output counterpart of _Light_.
- [Temperature](examples/Temperature/Temperature.ino) measures temperature and humidity with a DHT21, through an external library (Adafruit's DHT sensor library): how to use a sensor library in an iotsa module.
- [Input](examples/Input/Input.ino) is a dimmer set by hand: a button and a rotary encoder, read by the _iotsaInput_ module, dim an LED, and a web page shows the values.
- [Led](examples/Led/Led.ino) lets the outside world flash the LED (colour, on/off times,
  duration) over the web, REST, CoAP and a BLE service of its own. Shows how to extend a
  standard module (_iotsaLed_), and a module that only makes something happen.
- [LedBattery](examples/LedBattery/LedBattery.ino) is _Led_ on a battery-powered device:
  _iotsaBattery_ measures the battery and USB voltages, the device can sleep, and a BLE
  client can switch it into configuration mode, so it doesn't need WiFi all the time.
- [Button](examples/Button/Button.ino) and [Ringer](examples/Ringer/Ringer.ino) are a doorbell over HTTP. _Button_ calls a configurable URL when its button is pressed; _Ringer_ sounds a buzzer when asked, on its web page, its REST API or its trigger `/ring`, which is the URL the button calls.
- [BLEButton](examples/BLEButton/BLEButton.ino) and [BLERinger](examples/BLERinger/BLERinger.ino) are the same doorbell over BLE, with the Immediate Alert Service, a Bluetooth standard. _BLERinger_ shows the alert on the status LED.
- [BLEManager](examples/BLEManager/BLEManager.ino) manages other iotsa devices over BLE: it finds them, and can identify or reboot them, switch their WiFi on or off, or confirm a mode change. A bridge between WiFi and devices that only have BLE.

Board/feature build coverage (not tutorial material) lives separately, under `tests/`, see `extras/python/gen_build_matrix.py`. Sketches under active development, not yet stable enough to be doc-grade material (e.g. [Infra](sandbox/Infra), a minimal scaffold for infrastructure and transport work), live under `sandbox/` instead -- still built on every push, just without `examples/`'s promise of being a stable starting point to copy.

## more projects using iotsa

Here are some projects that use iotsa, and that can also be used as further examples (_Note that as of this writing not all projects may be publicly accessible yet_):

### [lissabon](http://github.com/cwi-dis/lissabon)

![build-platformio](https://github.com/cwi-dis/lissabon/workflows/build-platformio/badge.svg)

This is a whole set of appliances aimed at very low power overhead control of lighting in an off-grid situation (think: battery-backed solar power holiday home). Dimmers, LED strip controllers, remotes.

### [iotsaGPIO](http://github.com/cwi-dis/iotsaGPIO)

![build-platformio](https://github.com/cwi-dis/iotsaGPIO/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaGPIO/workflows/build-arduino/badge.svg)

Allows web access (or REST access) to analog and digital input and output pins.

### [iotsaDisplayServer](http://github.com/cwi-dis/iotsaDisplayServer)

![build-platformio](https://github.com/cwi-dis/iotsaDisplayServer/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDisplayServer/workflows/build-arduino/badge.svg)

Drives an LCD display, such as an i2c 4x40 character module. Support for a buzzer (to attract user attention) and buttons (programmable to trigger actions by accessing programmable URLs) is included.

### [iotsaOLEDServer](http://github.com/cwi-dis/iotsaOLEDServer)

![build-platformio](https://github.com/cwi-dis/iotsaOLEDServer/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaOLEDServer/workflows/build-arduino/badge.svg)

Drives an OLED display, such as an Adafruit SSD1306. Same external API as _iotsaDisplayServer_, but also allows Bluetooth LE control.

### [iotsaMotorServer](http://github.com/cwi-dis/iotsaMotorServer)

![build-platformio](https://github.com/cwi-dis/iotsaMotorServer/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaMotorServer/workflows/build-arduino/badge.svg)

Drives one or more stepper motors. Schematics and 3D models are included for a device to lift an object (such as a plant in a pot) to a height that can be changed through the web.

### [iotsaNeoClock](http://github.com/cwi-dis/iotsaNeoClock)

![build-platformio](https://github.com/cwi-dis/iotsaNeoClock/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaNeoClock/workflows/build-arduino/badge.svg)

A clock comprised of 60 NeoPixel LEDs. Shows the time, but can also show programmable patterns (as alerts) and temporal information (such as expected rainfall for the coming hour). Schematics and building instructions included.

### [iotsaDoorOpener](http://github.com/cwi-dis/iotsaDoorOpener)

![build-platformio](https://github.com/cwi-dis/iotsaDoorOpener/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDoorOpener/workflows/build-arduino/badge.svg)

Operates a solenoid to open a door. On web access, or when an RFID tag (such as a keychain fob or a mifare contactless transport card) is presented. RFID cards are programmable (over the net, or using a special "learn" card). A web request can be sent to a programmable URL when a card is presented.

### [iotsaSmartMeter](http://github.com/cwi-dis/iotsaSmartMeter)

![build-platformio](https://github.com/cwi-dis/iotsaSmartMeter/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaSmartMeter/workflows/build-arduino/badge.svg)

Reads electricity and gas usage of a dutch Smart Meter through the standardised P1 port and makes the data available on the net.

### [iotsaDoorbellButton](http://github.com/cwi-dis/iotsaDoorbellButton)

![build-platformio](https://github.com/cwi-dis/iotsaDoorbellButton/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDoorbellButton/workflows/build-arduino/badge.svg)

Very simple server that emits a GET request when a button is pressed. Uses capabilities.

### [iotsaDoorbellRinger](http://github.com/cwi-dis/iotsaDoorbellRinger)

![build-platformio](https://github.com/cwi-dis/iotsaDoorbellRinger/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDoorbellRinger/workflows/build-arduino/badge.svg)

Very simple server that sounds a buzzer when a GET request is received. Uses capabilities. 

### [iotsaDMXLedstrip](http://github.com/cwi-dis/iotsaDMXLedstrip)

![build-platformio](https://github.com/cwi-dis/iotsaDMXLedstrip/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDMXLedstrip/workflows/build-arduino/badge.svg)

A driver for NeoPixel or similar LED strips to use for theatre lighting. The individual LEDs are controllable using the Art-Net DMX protocol, which is supported by many theatre lighting consoles or software such as the open source QLC+.

### [iotsaDMXSensor](http://github.com/cwi-dis/iotsaDMXSensor)

![build-platformio](https://github.com/cwi-dis/iotsaDMXSensor/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDMXSensor/workflows/build-arduino/badge.svg)

Reads sensor values from Estimote Bluetooth LE sensors and makes these available as DMX slider values (using DMX over Art-Net over WiFi). Can be used to input sensor values into a standard theatre lighting console or software such as QLC+.

### [iotsaEstimotes](http://github.com/cwi-dis/iotsaEstimotes)

![build-platformio](https://github.com/cwi-dis/iotsaEstimotes/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaEstimotes/workflows/build-arduino/badge.svg)

A gateway that listens for Estimote Bluetooth LE sensors and exposes their readings as REST values on the net.

### [iotsa433](http://github.com/cwi-dis/iotsa433)

![build-platformio](https://github.com/cwi-dis/iotsa433/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsa433/workflows/build-arduino/badge.svg)

Server to receive and transmit 433 MHz home automation signals (the cheap wireless remote controlled sockets, dimmers, garage doors, etc).

### [iotsaDataLogger](http://github.com/cwi-dis/iotsaDataLogger)

![build-platformio](https://github.com/cwi-dis/iotsaDataLogger/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaDataLogger/workflows/build-arduino/badge.svg)

Read an analog sensor at an interval that can be specified. Sensor readings are saved to flash memory, together with the timestamp of reading.
A realtime clock (RTC) can be used to keep time, which will sync with NTP whever WiFi is available. The program can hibernate (using micro-amperes) between readings.
This program is intended to take a few measurements per day over long periods of time (months).

### [iotsaRGBWSensor](http://github.com/cwi-dis/iotsaRGBWSensor)

![build-platformio](https://github.com/cwi-dis/iotsaRGBWSensor/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaRGBWSensor/workflows/build-arduino/badge.svg)

Read current ambient light level (RGB and White, color temperature) using a VEML6040 sensor.
Can be used to calibrate RGB, RGBW and RGBWW led strips.


## hardware

Folder _extras/fritzing_ contains design (circuit and PCB, and partial breadboard layout)
for a PCB based on an ESP-12 that makes a nice iotsa hardware platform. You need the open source [Fritzing](http://fritzing.org/home/) tool to open these files.

The iotsa board has the ESP-12, a 3.3v regulator (so it can be powered with a standard 5v-16v power
supply) and a free section with enough space for a DIP IC (up to DIP-20) and/or
a few discrete components. All easily usable GPIO pins are also available.
There's also room for an FTDI header (so you can reprogram the board if you have bricked it with over-the-air programming) and _reset_ and _program_ buttons.

The resistors are all pullups and pulldowns, so their values are not very critical. With one exception: if you use an ESP-12S module R3 (the program button pulldown) must be around 3K3, a 10K resistor will not work with the ESP-12S (as was found experimentally, since 10K works just fine with an ESP-12F).

The v3 board is about 6.35x4.35cm in size, with the ESP-12 antenna sticking out
0.5cm.

There is no board for the ESP32, but iotsa is known to work with many standard esp32 boards such as the Lolin boards or the Esp32Thing. There
are too many different esp32 boards with different features sets that are useful for different projects...

### Case

Extras also has a file [extras/iotsaCase.scad](extras/iotsaCase.scad) that contains the OpenSCAD source code to
3D-print your own box to fit a iotsa board. Examine the source, there are various
ways to adjust the design (to add extra holes, or determine thickness of
the box, or make the box fit a iotsa board with some of the experimental area rows
removed.

The 3D-printable case for a iotsa board is also available at Thingyverse: <http://www.thingiverse.com/thing:2303793>.

## Creating a new release

Notes to self, mainly, on creating a new release:

- Cleanup `CHANGELOG.md`.
- Update version number in `library.properties` and `library.json`.
- Run

 ```
 IOTSA_FULL_VERSION=vX.Y.Z python3 extras/python/mkversionh.py
  sh ./extras/ignore-version-changes.sh off
  ```
- Examine `extras/python/iotsa/version.py` and `src/iotsaVersion.h` to ensure the script has not created any syntax errors or such:-)
- Update `CHANGELOG.md` with version number and date.
- Commit, Push
- If a real version: merge into `master` and tag on master (otherwise tag on `develop`)
- This should trigger the release build via GitHub Actions.
- After a real release:
	- Update `CHANGELOG.md` again on `develop`.
	- Ensure release is available on platformIO library registry.
	- Ensure release is available in Arduino library registry. The log file is at <https://downloads.arduino.cc/libraries/logs/github.com/cwi-dis/iotsa/>
- You probably want to run `sh ./extras/ignore-version-changes.sh on` again after all is done.
