# iotsa - Internet of Things server architecture

This library contains a framework to easily create esp8266-based or esp32-based web servers that can interface to all sorts of sensors and actuators. The servers can be REST-compatible, and COAP-compatible, including
seamless integration with the [Igor home automation server](https://github.com/cwi-dis/igor). 


Home page is <https://github.com/cwi-dis/iotsa>. 
This software is licensed under the [MIT license](LICENSE.txt) by the CWI DIS group, <http://www.dis.cwi.nl>.

## Installation and use for developers

Iotsa can be used with both the Arduino IDE and with the PlatformIO build system (which can be used from within Atom or VSCode or from the command line). The Arduino IDE is easiest to get started with, but PlatformIO is more powerful if you want to target multiple device types, use _Git_ integration, etc.

### Arduino IDE

Download the zipfile (via <https://github.com/cwi-dis/iotsa>) and install into Arduino IDE with _Sketch_ -> _Include Library_ -> _Add .ZIP Library..._ . (If you downloaded the zipfile through github you may have to rename the directory because it may be called something like _iotsa___master_).

Build the _Hello_ example (_File -> Examples -> iotsa -> Hello_) and flash it onto an ESP-12 or similar board.

**Note:** the structure of the iotsa examples may be slightly different than what you are used to for Arduino examples: the `Hello.ino` file is basically empty, and the code is contained in the `mainHello.cpp` file, which is in a separate tab in the Arduino IDE.

### PlatformIO

The _iotsa_ library should be known to the library manager, so simply adding it to your _platformio.ini_ file should do the trick for adding the iotsa framework to your project..

To build an example you can open the `iotsa` source directory in _VSCode_ or _Atom_ and look at the `[env:nodemcuv2-example-skeleton]` section. Replace the references to _skeleton_ with the name of the example you want to build.

But: each example in the _examples_ folder also has its own _platform.ini_ file to build it. So you can also open `iotsa/examples/Hello` in _VSCode_ or _Atom_ and build it, or use the command line:

```
$ cd iotsa/examples/Hello
$ platformio run --target build
$ platformio run --target upload
```

### Both Arduino IDE and PlatformIO

On reboot, the board will first initialize the SPIFFS flash filesystem (if needed) and then create a WiFi network with a name similar to _config-iotsa1234_. Connect a device to that network and visit <http://192.168.4.1>. Configure your device name (at <http://192.168.4.1/config>) and WiFi name and password (at <http://192.168.4.1/wificonfig>), and after reboot the iotsa board should connect to your network and be visible as <http://yourdevicename.local>.

When the device is running normally you can visit <http://yourdevicename.local/config> and request the device to go into configuration mode, or to do a factory reset. After requesting this you have 5 minutes to power cycle the device to make it go into configuration mode again (see previous paragraph) or do a complete factory reset. When in configuration mode you have five minutes to change the configuration (device name, WiFi name, password, maybe other parameters) before the device reverts to normal operation. The idea behind this sequence (_request configuration mode_, then _power cycle_, then _change parameters_) is that you need both network acccess and physical access before you can do a disruptive operation on the device.

### Build time options

A number of features of the iotsa framework can be enabled and disabled selectively at build time. These features are encoded in `iotsaBuildOptions.h`. 

The general naming convention is that a feature _WIFI_ will be triggered by a define `IOTSA_WITH_WIFI`. If the _WIFI_ feature is enabled by default (which happens to be the case for _WIFI_, obviously) that can be overridden by defining `IOTSA_WITHOUT_WIFI` at compile time of the iotsa library.

When using the Arduino IDE you can edit this file to change the options, by commenting out the defines for features you do not need.

When using Platformio you can also use the `build_flags` in _platformio.ini_. For example, to enable COAP and disable REST:

```
	build_flags = -DIOTSA_WITHOUT_REST -DIOTSA_WITH_COAP
```

The following features are defined:

- `DEBUG` Enables logging and debugging messages to the serial line. Default on.
- `WIFI` Enables the WiFi network interface. Default on.
- `HTTP` Enables the web server infrastructure. Default on.
- `HTTPS` Enables the secure web server infrastructure. Default off. Exclusive with `HTTP`.
- `WEB` Enables the user-oriented web interfaces. Default on. Requires `HTTP` or `HTTPS`.
- `API` Enables the application-oriented interfaces. Default on. Requires `REST` or `COAP`.
- `REST` Enables the http(s) based REST application interfaces. Default on. Requires `HTTP` or `HTTPS`.
- `COAP` Enables the udp-based COAP application interfaces. Default off.

There are a few more that are not very important, please inspect `iotsaBuildOptions.h`.

## HTTPS support

If a iotsa application is built with `IOTSA_WITH_HTTPS` if will initially use a builtin key and certificate. This is **NOT** secure, because the key and certificate are contained in the github source repository, and therefore known to the world.

After building and flashing your software for the first time you should create a new, unique, key and certificate. There are three ways to do this, using scripts in the `extras` directory:

- `extras/name-self-signed-cert.sh` creates a self-signed certificate. This can be copied into your source code (before building and flashing).
- `extras/make-csr-step{1,2,3}.sh` these create a key and self-signed or CA-signed certificate that can be uploaded to the iotsa device using `iotsaControl`.
- `extras/make-igor-signed-cert.sh` creates a key and certificate signed by your [Igor](https://github.com/cwi-dis/igor) CA and uploads it to your device.

Note that HTTPS support here refers to iotsa as a server only, HTTPS client support (for the _iotsaButton_ and _iotsaRequest_ modules) is completely independent.

## Controlling iotsa devices

### iotsaControl

There is a command line helper program _iotsaControl_. It will be documented here at some point. For now, install _iotsaControl_ with the following commands:

```
cd extras
python setup.py build
sudo python setup.py install
```

Then you can get a list of the available commands with `iotsaControl help` and a list of the available options with `iotsaControl --help`.


### OTA programming

If you have enabled over-the-air programming <http://yourdevicename.local/config> will also allow you to request the device to go into programmable mode. Again, you have 5 minutes to power cycle and then 5 minutes to reprogram:

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

      or, on either, use the convenience script

      ```
      [...]/iotsa/extras/refreshOTA.sh
      ```
  - Use the normal _Upload_ command to flash your new program.
- _For PlatformIO_: 
  - Visit <http://yourdevicename.local> and note the IP address.
  - Edit `platform.ini` and add the `upload_port` setting with the correct host name or IP address,
  - or upload with the following command line:
  ```
  platformio run -t upload --upload-port yourdevicename.local
  ```

- _For PlatformIO, using iotsaControl_:
	- Build using `platformio run` or the plaformio IDE integration commands.
	- `iotsaControl --target yourdevicename.local otaWait ota ./.pioenvs/nodemcuv2/firmware.bin`
	- Power cycle the device when _iotsaControl_ asks you to do so.
	
## General design philosophy

Unlike most Arduino libraries and frameworks Iotsa does expose some of its C++ interfaces, but for simple applications you do not have to worry about this.

Iotsa has two main types of objects:

* `IotsaApplication`, of which there is only one, which is the web server and the container for the plugin modules.
* `IotsaMod` which is a plugin module and of which there can be many. Each plugin module provides a web interface (usually with a HTML form to allow control over it) plus some functionality. The `IotsaMod` class is subclassed to provide specific functionality. 

  One subclass that is always used is the `IotsaWifiMod`, which provides the functionality to connect to a specific Wifi network (after the user has provided the name and password). 
  
  One that is often used is `IotsaSimpleMod` which allows you to write two functions to implement your own functionality (your reason for actually using Iotsa).

### Do-nothing application

You create a global variable `application` of type `IotsaApplication` to hold the basic implementation of your service framework, plus the `ESP8266WebServer` object on which the application will serve. You also create one `IotsaWifiMod` and link it to the application so the end user can configure the WiFi network to join and such:

```
IotsaApplication application("Iotsa Hello World Server");
IotsaWifiMod wifiMod(application);

```

In your `setup()` function you call `app.setup()` and `app.serverSetup()` which will initialize the Iotsa framework:

```
void setup(void){
  application.setup();
  application.serverSetup();
  // Add your own setup code here....
}
```

In your `loop()` function you call `app.loop()`. This will take care of handling requests and every thing else that is needed to make the framework work:

```
void loop(void){
  application.loop();
  // Add your own loop code here....
 }
```

You have now created a Iotsa server that does absolutely nothing. 

The [Skeleton](examples/Skeleton/Skeleton.ino) example does exactly this (but there are various `#define` statements at the top that you either turn into `#undef`, or keep to give you extra functionality).

### Hello world application

One step more functionality is provided by the [Hello](examples/Hello/Hello.ino) example. It registers a handler function for the `/hello` URL (which provides a form through which a user can type in his or her name) and an info function that returns a paragraph (or more) on the home page `/`:

```
void
helloHandler() {
  // Parse form arguments using server->args() and server->arg(...).
  String message = "<html>...construct html page...</html>";
  server->send(200, "text/html", message);
}

String helloInfo() {
  String rv = "<p>See <a href=\"/hello\">/hello</a> for info.</p>";
  return rv;
}

```

These two functions (and the URL) are registered in the application as a `IotsaSimpleMod`:

```
IotsaSimpleMod helloMod(application, "/hello", helloHandler, helloInfo);
```

### Another hello world application

From a functionality point of view the [HelloCpp](examples/HelloCpp/HelloCpp.ino) program is identical to _Hello_, but it is structured differently, as a C++ class. This is a bit more difficult to read (when you are used to standard Arduino programming and not C++ programming) but has the advantage that the functionality can easily be reused in other servers. Actually, most standard modules (below) started their life this way. Also, if you want to create a device without a user interface (REST or COAP only) you should follow this pattern.

You declare the class for your functionality:

```
class IotsaHelloMod : public IotsaMod {
public:
  IotsaHelloMod(IotsaApplication &_app) : IotsaMod(_app) {}
  void setup();
  void serverSetup();
  void loop();
  String info();
private:
  void handler();
};

```

Then you implement the 5 methods, of which only `serverSetup` needs explanation: it is called when this module is added to the application, and should register the URL:

```
void IotsaHelloMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/hello", std::bind(&IotsaHelloMod::handler, this));
}
```

Finally you create a single object of your new `IotsaHelloMod` type and register it to the application:

```
IotsaHelloMod helloMod(application);
```

Iotsa will now take care of calling your classes `IotsaHelloMod ::setup()`, `IotsaHelloMod ::serverSetup()` and `IotsaHelloMod ::loop()` methods at the right time without you needing to add any code to the normal `setup()` and `loop()` functions. Which means that this implementation of _Hello_ can be combined with as many other modules as you want, just by adding that 1-line declaration.

### Hello World with an API

If you want your server to have a programming interface (either REST to access it over HTTP/TCP or COAP to access it over UDP) you use the base class `IotsaApiMod`. You provide methods for handling _GET_, _PUT_ and _POST_ requests (only for the ones you need) and register your API endpoint in your _serverSetup_. 

The [HelloApi](examples/HelloApi/HelloApi.ino) example has the full details, but here are the _GET_ and _PUT_ methods:

```
bool IotsaHelloMod::getHandler(const char *path, JsonObject& reply) {
  reply["greeting"] = greeting;
  return true;
}

bool IotsaHelloMod::putHandler(const char *path, const JsonVariant& request, JsonObject& reply) {
  JsonVariant arg = request["greeting"];
  if (arg.is<char*>()) {
    greeting = arg.as<String>();
    return true;
  }
  return false;
}
```

And here is the modified _serverSetup_ method:

```
void IotsaHelloMod::serverSetup() {
  // Setup the web server hooks for this module.
  server->on("/hello", std::bind(&IotsaHelloMod::handler, this));
  api.setup("/api/hello", true, true);
  name = "hello";
}
```

### HTML encoding

Strings that are interpolated into the HTML returned from the `info()` or `handler()` function must be ampersand-encoded. There is a static method in `IotsaMod` to do this for you:

```
String IotsaMod::htmlEncode(String data);
```

### Next steps

The [Hello](examples/Hello/Hello.ino) and [HelloCpp](examples/HelloCpp/HelloCpp.ino) examples shows how to do basic interaction with the user (through a browser form, and through information on the how page). [HelloApi](examples/HelloApi/HelloApi.ino) shows how to add an API to your service. Various types of access control are demonstrated in the [HelloUser](examples/HelloUser/HelloUser.ino), [HelloPasswd](examples/HelloPasswd/HelloPasswd.ino), [HelloRights](examples/HelloRights/HelloRights.ino) and [HelloToken](examples/HelloToken/HelloToken.ino) examples.

To use this to create an interface to some bit of sensor hardware simply add the usual code to your `setup()` and `loop()` functions, and store the sensor value that you read in your `loop()` in a global variable. Pick up the value of this variable in your handler or info function and format it in HTML. [Light](examples/Light/Light.ino) and [Temperature](examples/Temperature/Temperature.ino) are example programs of this type.

To interface to an actuator you present an HTML form in your handler, and store the user-supplied value in a global variable. Your `setup()` and `loop()` functions are again as usual, but in `loop()` you pick up the value from the global variable. [Led](examples/Led/Led.ino) is an example program of this type.

Slightly more elaborate API examples are [Button](examples/Button/Button.ino) and [Ringer](examples/Ringer/Ringer). _Button_ waits for a button to be pressed on the device and then makes a call to a user-specifyable URL. If that URL happens to point to _Ringer_ it will in turn sound a buzzer.

## User interface and operation

## Core API

### iotsa.h

This file declares the `IotsaApplication` and `IotsaMod` classes explained earlier. In addition it declares a class `IotsaAuthMod` which is a subclass of `IotsaMod` (so it has all the functionality of a normal module, like the handler) but it can be used as the _authenticator_ for another module. This allows the other module to use access control: it will only work after the user has provided a username/password combination, or pressed a certain button (probably key-operated) or any other means of authentication you can think of.

Here are the constructors of the three classes:

```
IotsaApplication(const char *_title);
IotsaMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false);
IotsaAuthMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false);
```

The optional `early` argument signifies that the module should be initialized early, this is generally used only by the WiFi module.

### iotsaApi.h

Contains a class `IotsaApiMod`, a subclass of `IotsaMod`. Subclass this to implementing modules that also provide a REST interface. The class provides a method to register your API endpoint:

```
void apiSetup(const char* path, bool get=false, bool put=false, bool post=false);
```

Call this in your `serverSetup()` method. Call multiple times if your class implements multiple endpoints.
You should also provide the following methods to implement your `GET`, `PUT` and `POST` methods (for the ones for which you passed `true` in your `apiSetup` call):

```
bool getHandler(const char *path, JsonObject& reply);
bool putHandler(const char *path, const JsonVariant& request, JsonObject& reply);
bool postHandler(const char *path, const JsonVariant& request, JsonObject& reply);
```
These methods are called on incoming REST or COAP calls. The request parameter object (or array, or value) is in `request`, store results in `reply` (which is already an initialized empty object). Return `true` for success, `false` for any failure (which will not return the reply object to the caller).

The actual implementations are in _iotsaRestApi.h_ and _iotsaCoapApi.h_, but these are transparently renamed or combined.

### iotsaConfig.h

Handles general configuration, factory reset and (if enabled) over-the-air programming.
This is technically a module, but unlike other modules it is not really optional. It should _not_ be instantiated in your
program, this happens automatically. This module also opens and/or initializes the SPIFFS filesystem.

If the iotsa server is operating in normal (production) mode a user can access URL `/config` to request configuration mode, factory reset or ota-programming mode. The user must then turn power off and on again within 5 minutes to switch the iotsa server to its new mode, for another 5 minutes. If nothing happens during this period the server reboots and reverts to normal mode.

The intention of requiring a power cycle is that any "dangerous" operation requires both network access (to request the operation) and physical access (to turn power on and off).

On a factory reset all configuration information (literally: _all_) is forgotten and the iotsa device is completely new again.

In ota-programming mode the device functions normally, but can also be reprogrammed using the Arduino IDE and its OTA facilities.

The module also provides a REST api on `/api/config` (and this api depends on whether in configuration mode or not).

### iotsaWifi.h

Handles WiFi configuration.
This is technically a module, but unlike other modules it is not really optional for this release .You must instantiate it in your program.

A iotsa device can join an existing WiFi network (normal WiFi mode) or create a temporary network as an Access Point (private WiFi mode). 
In private mode the device does not connect to a WiFi network, but in stead creates its own network (as a base station) with a name starting with "_config-_". The user can now connect a device to this network and visit `http://192.168.4.1/wificonfig`. Here it is possible to set the normal WiFi network to connect to and the password.

If a device is new (it has no WiFi network name) it will enter private mode automatically. If a device cannot find its configured WiFi network it will enter private mode for 5 minutes and then reboot and retry joining the configured network.

If a device has a WiFi network configured but it cannot join this network after trying for 5 minutes it will go to private network mode.

If a device is in normal WiFi mode the WiFi parameters can only be changed if the device is in configuration mode.

The module also provides a REST api on `/api/wificonfig` (and this api depends on whether in configuration mode or not).

The module can be disabled (by building with _IOTSA\_WITHOUT\_WIFI_) but until other network interface modules are implemented this is not very useful.

### iotsaSimple.h

Allows implementing a module using two simple C functions. See the _hello_ section above for details.


### iotsaConfigFile.h

Two classes to save and load configuration variables to a file (on the SPIFFS file system in the flash memory chip). `IotsaConfigFileLoad` is used to load configuration values and `IotsaConfigFileSave` to store them. 

The general paradigm is that in your `setup()` function you create a local (stack based) loader variable with 

```
IotsaConfigFileLoad cf("/config/filename.cfg");
```
and then repeatedly call `cf.get("name", variable, default)` for each configuration variable. Variables can be of type `int`, `float` or `String`.

To save configuration values you call

```
IotsaConfigFileSave cf("/config/filename.cfg");
```

and then repeatedly call `cf.put("name", variable)`.


## available standard modules

### iotsaBattery.h

Allows sleeping, hibernating and shutting down WiFi of your iotsa device to conserve battery power.

### iotsaBLEServer.h

Allows a iotsa device to export an API as a Bluetooth LE service. ESP32 only.
Can be used with the module (which allows setting some BLE parameters such as
advertising interval through the web or REST interface) or without it.

### iotsaButton.h

A generalized module for handling buttons and switches attached to GPIO pins. Uses _iotsaRequest.h_ to enable REST calls when buttons are pressed.

### iotsaCapabilities.h

An authentication module (_IotsaAuthMod_) that trusts an external server, 
the _issuer_, to determine access rights to this board.

The _issuer_ and this board share a secret key.

More documentation will be forthcoming.

### iotsaFiles.h

Allows read access to files stored in `/data` on the SPIFFS file system (in the flash memory chip). Could be used for a simple web server. Requires _IOTSA\_WITH\_WEB_.

### iotsaFilesUpload.h

Allows write access to files stored in `/data` on the SPIFFS filesystem (where _iotsaFiles_ reads from), through `POST` requests to the `/upload` URL. Requires _IOTSA\_WITH\_WEB_.

### iotsaFilesBackup.h

Creates a backup of the complete SPIFFS filesystem (including `/data` and `/config`) as a tarfile when you access URL `/backup.tar`. Can be used to clone iotsa devices. Requires _IOTSA\_WITH\_WEB_.

### iotsaLed.h

Allows showing static colors and repeating patterns on a NeoPixel LED. By default does not provide a web interface, only an API `set(rgb, onDuration, offDuration, count)` for use in your program. But see the _Led_ example for providing a web interface.

The iotsaLed module also implements the `iotsaStatusInterface` protocol, and shows status information during the boot sequence and when the iotsa board is running in a nonstandard mode (configuration mode, OTA mode, etc).

The module does not provide a user-visible endpoint or REST api, but can be used as a base class for this. See [examples/Led](examples/Led) for an example.

### iotsaLogger.h

Replaces the standard `Serial` object by an object that stores data in a memory buffer, and allows access to that memory buffer through the URL `/logger`. The buffer is tiny, 4Kb, but this allows some limited debugging over WiFi. Requires _IOTSA\_WITH\_HTTP_ or _IOTSA\_WITH\_HTTPS_.

### iotsaNothing.h

A module that does nothing. Use this as the basis of your own modules. Provides a user interface at `/nothing` and a REST interface on `/api/nothing`.

### iotsaNtp.h

A module that contacts an NTP server to set the local clock. Accessing URL `/ntpconfig` allows setting the NTP server to use, as well as the timezone.

The module provides an API to get access to the time information:

```
unsigned long utcTime();	// Seconds since 1-Jan-1970, UTC (unix time)
unsigned long localTime();	// Seconds since 1-Jan-1970, local time zone
int localSeconds();			// Local time-of-day, seconds (0-59)
int localMinutes();			// Local time-of-day, minutes (0-59)
int localHours();			// Local time-of-day, hours (0-23)
int localHours12();			// Local time-of-day, hours (0-11)
bool localIsPM();			// AM/PM indicator

```

Provides a user interface at `/ntp` and a REST interface at `/api/ntp`.

### iotsaOta.h

Allows Over-the-air reprogramming of a iotsa server. After ota-programming has been enabled the device will show up (for 5 minutes) in the Arduino IDE, menu _Tools_ -> _Port_, under the _Network Ports_ section. Select it, and press the checkmark on your sketch to upload. Requires _IOTSA_WITH_HTTP_ or _IOTSA_WITH_HTTPS_.

### iotsaRequest.h

Allows issuing of _http_ and _https_ requests. Carrying username/password credentials and tokens is possible. This is not a complete module in itself, but provides the building blocks to allow other modules to easily add the ability to send (configurable) web requests.

### iotsaUser.h

An authentication module (_IotsaAuthMod_) that stores a single username and password. Other modules can then specify they are only accessible after the user authenticates with this username/password combination.

Accessing URL `/users` allows changing the password, and if the password has never been set the default password is shown whenever the device is booted in configuration mode.

### iotsaMultiUser.h

An authentication module (_IotsaAuthMod_) that stores a multiple usernames, passwords
and rights. Other modules can then specify they are only accessible after the user authenticates with a username/password combination that has the correct _right_ in
its set of rights.

Accessing URL `/users` allows adding and changing users. A factory-installed device
has no users, and allows all rights always.


## sample programs
- [Skeleton](examples/Skeleton/Skeleton.ino) is a good starting point for your own applications.
- [Hello](examples/Hello/Hello.ino) is the simplest "Hello, user" server.
- [HelloCpp](examples/HelloCpp/HelloCpp.ino) is the same, but implemented using C++ class declarations.
- [Light](examples/Light/Light.ino) measures ambient light level with an LDR connected to the analog input.
- [Temperature](examples/Temperature/Temperature.ino) measures temperature with a slightly more complicated sensor, a DHT21.
- [Led](examples/Led/Led.ino) controls the color of a NeoPixel LED, and can set up repeating patterns. Uses _iotsaLed_ module.
- [BLELed](examples/BLELed/BLELed.ino) controls the color of a NeoPixel LED. Can be controlled over Bluetooth LE when built for an esp32 board.
- [Ringer](examples/Ringer/Ringer.ino) 
- [HelloPasswd](examples/HelloPasswd/HelloPasswd.ino) The same "Hello" server, but now using a _IotsaAuthMod_ for access control (you need to provide username "admin" and password "admin" to change the greeting name). Builds with HTTPS support by default (when using platformIO).
- [HelloUser](examples/HelloUser/HelloUser.ino) Another "Hello" server that needs authentication, but this time using _IotsaUserMod_ so the password can be changed. Builds with HTTPS support by default (when using platformIO).
- [HelloToken](examples/HelloToken/HelloToken.ino) Another "Hello" server that needs authentication, but this time using a token, where tokens can be created that give certain rights. Not very useful except as an example. Builds with HTTPS support by default (when using platformIO).
- [HelloUser](examples/HelloUser/HelloUser.ino) Another "Hello" server that needs authentication, but this time using _IotsaUserMod_ so the password can be changed. Builds with HTTPS support by default (when using platformIO).
- [Log](examples/Log/Log.ino) Example of using the _iotsaLogger_ module.

## more projects using iotsa

Here are some projects that use iotsa, and that also be used as further examples (_Note that as of this writing not all projects may be publicly accessible yet_):

* [iotsaGPIO](http://github.com/cwi-dis/iotsaGPIO): allows web access (or REST access) to analog and digital input and output pins.
* [iotsaDisplayServer](http://github.com/cwi-dis/iotsaDisplayServer): Drives an LCD display, such as an i2c 4x40 character module. Support for a buzzer (to attract user attention) and buttons (programmable to trigger actions by accessing programmable URLs) is included.
* [iotsaMotorServer](http://github.com/cwi-dis/iotsaMotorServer): Drives one or more stepper motors. Schematics and 3D models are included for a device to lift an object (such as a plant in a pot) to a height that can be changed through the web.
* [iotsaNeoClock](http://github.com/cwi-dis/iotsaNeoClock): A clock comprised of 60 NeoPixel LEDs. Shows the time, but can also show programmable patterns (as alerts) and temporal information (such as expected rainfall for the coming hour). Schematics and building instructions included.
* [iotsaDoorOpener](http://github.com/cwi-dis/iotsaDoorOpener): Operates a solenoid to open a door. On web access, or when an RFID tag (such as a keychain fob or a mifare contactless transport card) is presented. RFID cards are programmable (over the net, or using a special "learn" card). A web request can be sent to a programmable URL when a card is presented.
* [iotsaSmartMeter](http://github.com/cwi-dis/iotsaSmartMeter): Reads electricity and gas usage of a dutch Smart Meter through the standardised P1 port and makes the data available on the net.
* [iotsaDoorbellButton](http://github.com/cwi-dis/iotsaDoorbellButton) and [iotsaDoorbellRinger](http://github.com/cwi-dis/iotsaDoorbellRinger) are two very simple REST services that together form a door bell. 
* [iotsaLedstrip](http://github.com/cwi-dis/iotsaLedstrip): A driver for NeoPixel or similar LED strips. This application is mainly intended to use the LED strips for lighting, so you are able to adjustcolor temperature, intensity, etc.
* [iotsaDMXLedstrip](http://github.com/cwi-dis/iotsaDMXLedstrip): A driver for NeoPixel or similar LED strips to use for theatre lighting. The individual LEDs are controllable using the Art-Net DMX protocol, which is supported by many theatre lighting consoles or software such as the open source QLC+.
* [iotsaDMXSensor](http://github.com/cwi-dis/iotsaDMXSensor): Reads sensor values from Estimote Bluetooth LE sensors and makes these available as DMX slider values (using DMX over Art-Net over WiFi). Can be used to input sensor values into a standard theatre lighting console or software such as QLC+.

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

There is no board for the ESP32, but iotsa is known to work with many standard esp32 boards such as the Lolin boards or the Esp32Thing.

### Case

Extras also has a file [extras/iotsaCase.scad](extras/iotsaCase.scad) that contains the OpenSCAD source code to
3D-print your own box to fit a iotsa board. Examine the source, there are various
ways to adjust the design (to add extra holes, or determine thickness of
the box, or make the box fit a iotsa board with some of the experimental area rows
removed.

The 3D-printable case for a iotsa board is also available at Thingyverse: <http://www.thingiverse.com/thing:2303793>.
