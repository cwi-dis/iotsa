# iotsa - Internet of Things server architecture

This library contains a framework to easily create esp8266-based web servers that can interface to all sorts of sensors and actuators. At the moment the servers can be partially REST-compatible, more support for this will be added later, including
seamless integration with the [Igor home automation server](https://github.com/cwi-dis/igor). 

PCB design for a small board with an ESP-12 and room for additional hardware is included, see the _hardware_ section below. A 3D-printable case (which can be customized to the size you need, and with extra holes for wires or switches or LEDs or whatever) is included too. In case you already have the hardware: there is a [Getting Started with Iotsa](docs/gettingStarted.md) guide available too.

Home page is <https://github.com/cwi-dis/iotsa>. 
This software is licensed under the [MIT license](LICENSE.txt) by the CWI DIS group, <http://www.dis.cwi.nl>.

## Installation and use

Download the zipfile (via <https://github.com/cwi-dis/iotsa>) and install into Arduino IDE with _Sketch_ -> _Include Library_ -> _Add .ZIP Library..._ . (If you downloaded the zipfile through github you may have to rename the directory because it may be called something like _iotsa___master_).

Build the _Hello_ example and flash it onto an ESP-12 or similar board.

On reboot, the board will first initialize the SPIFFS flash filesystem (if needed) and then create a WiFi network with a name similar to _config-iotsa1234_. Connect a device to that network and visit <http://192.168.4.1>. Configure your device name, WiFi name and password, and after reboot the iotsa board should connect to your network and be visible as <http://yourdevicename.local>.

When the device is running normally you can visit <http://yourdevicename.local/wificonfig> and request the device to go back into configuration mode, or to do a factory reset. After requesting this you have 2 minutes to power cycle the device to make it go into configuration mode again (see previous paragraph) or do a complete factory reset. When in configuration mode you have two minutes to change the configuration (device name, WiFi name, password) before the device reverts to normal operation. The idea behind this sequence (_request configuration mode_, then _power cycle_, then _change parameters_) is that you need both network acccess and physical access before you can do a disruptive operation on the device.

If you have enabled over-the-air programming <http://yourdevicename.local/wificonfig> will also allow you to request the device to go into programmable mode. Again, you have two minutes to power cycle and then two minutes to reprogram:

* In _Tools_ -> _Port_ -> _Network Port_ select your device.
	* Sometimes ota-programmable devices are slow to appear because of mDNS issues. On a Mac, run the following command in a _Terminal_ window to speed up discovery:
	
	```
	dns-sd -B _services._dns-sd._udp.
	```
* Use the normal _Upload_ command to flash your new program.

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
ESP8266WebServer server(80);
IotsaApplication application(server, "Iotsa Hello World Server");
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
  // Parse form arguments using server.args() and server.arg(...).
  String message = "<html>...construct html page...</html>";
  server.send(200, "text/html", message);
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

From a functionality point of view the [HelloCpp](examples/HelloCpp/HelloCpp.ino) program is identical to _Hello_, but it is structured differently, as a C++ class. This is a bit more difficult to read (when you are used to standard Arduino programming and not C++ programming) but has the advantage that the functionality can easily be reused in other servers. Actually, most standard modules (below) started their life this way.

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
  server.on("/hello", std::bind(&IotsaHelloMod::handler, this));
}
```

Finally you create a single object of your new `IotsaHelloMod` type and register it to the application:

```
IotsaHelloMod helloMod(application);
```

Iotsa will now take care of calling your classes `IotsaHelloMod ::setup()`, `IotsaHelloMod ::serverSetup()` and `IotsaHelloMod ::loop()` methods at the right time without you needing to add any code to the normal `setup()` and `loop()` functions. Which means that this implementation of _Hello_ can be combined with as many other modules as you want, just by adding that 1-line declaration.

### HTML encoding

Strings that are interpolated into the HTML returned from the `info()` or `handler()` function must be ampersand-encoded. There is a static method in `IotsaMod` to do this for you:

```
String IotsaMod::htmlEncode(String data);
```

### Next steps

The [Hello](examples/Hello/Hello.ino) and [HelloCpp](examples/HelloCpp/HelloCpp.ino) examples shows how to do basic interaction with the user (through a browser form, and through information on the how page). To use this to create an interface to some bit of sensor hardware simply add the usual code to your `setup()` and `loop()` functions, and store the sensor value that you read in your `loop()` in a global variable. Pick up the value of this variable in your handler or info function and format it in HTML. [Light](examples/Light/Light.ino) is an example program of this type.

To interface to an actuator you present an HTML form in your handler, and store the user-supplied value in a global variable. Your `setup()` and `loop()` functions are again as usual, but in `loop()` you pick up the value from the global variable. [Led](examples/Led/Led.ino) is an example program of this type.

## User interface and operation

## Core API

### iotsa.h

This file declares the `IotsaApplication` and `IotsaMod` classes explained earlier. In addition it declares a class `IotsaAuthMod` which is a subclass of `IotsaMod` (so it has all the functionality of a normal module, like the handler) but it can be used as the _authenticator_ for another module. This allows the other module to use access control: it will only work after the user has provided a username/password combination, or pressed a certain button (probably key-operated) or any other means of authentication you can think of.

Here are the constructors of the three classes:

```
IotsaApplication(ESP8266WebServer &_server, const char *_title);
IotsaMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false);
IotsaAuthMod(IotsaApplication &_app, IotsaAuthMod *_auth=NULL, bool early=false);
```

The optional `early` argument signifies that the module should be initialized early, this is generally used only by the WiFi module.

### iotsaWifi.h

Handles WiFi configuration, factory reset and (if enabled) over-the-air programming.
This is technically a module, but unlike other modules it is not really optional. This module also opens and/or initializes the SPIFFS filesystem (for historical reasons).

If the iotsa server is operating in normal (production) mode a user can access URL `/wificonfig` to request configuration mode, factory reset or ota-programming mode. The user must then turn power off and on again within 2 minutes to switch the iotsa server to its new mode, for another 2 minutes. If nothing happens during this period the server reboots and reverts to normal mode.

The intention of requiring a power cycle is that any "dangerous" operation requires both network access (to request the operation) and physical access (to turn power on and off).

On a factory reset all configuration information (literally: _all_) is forgotten and the iotsa device is completely new again.

In ota-programming mode the device functions normally, but can also be reprogrammed using the Arduino IDE and its OTA facilities.

In configuration mode the device does not connect to a WiFi network, but in stead creates its own network (as a base station) with a name starting with "_config-_". The user can now connect a device to this network and visit `http://192.168.4.1/wificonfig`. Here it is possible to change the normal (production) WiFi network to connect to and the password.

If a device is new (it has no WiFi network name) it will enter configuration mode automatically. If a device cannot find its configured WiFi network it will enter configuration mode for 2 minutes and then reboot.

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

### iotsaFiles.h

Allows read access to files stored in `/data` on the SPIFFS file system (in the flash memory chip). Could be used for a simple web server.

### iotsaFilesUpload.h

Allows write access to files stored in `/data` on the SPIFFS filesystem (where _iotsaFiles_ reads from), through `POST` requests to the `/upload` URL.

### iotsaFilesBackup.h

Creates a backup of the complete SPIFFS filesystem (including `/data` and `/config`) as a tarfile when you access URL `/backup.tar`. Can be used to clone iotsa devices.

### iotsaLed.h

Allows showing static colors and repeating patterns on a NeoPixel LED. By default does not provide a web interface, only an API `set(rgb, onDuration, offDuration, count)` for use in your program. But see the _Led_ example for providing a web interface.

### iotsaLogger.h

Replaces the standard `Serial` object by an object that stores data in a memory buffer, and allows access to that memory buffer through the URL `/logger`. The buffer is tiny, 4Kb, but this allows some limited debugging over WiFi.

### iotsaNothing.h

A module that does nothing. Use this as the basis of your own modules.

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
### iotsaOta.h

Allows Over-the-air reprogramming of a iotsa server. After ota-programming has been enabled the device will show up (for 2 minutes) in the Arduino IDE, menu _Tools_ -> _Port_, under the _Network Ports_ section. Select it, and press the checkmark on your sketch to upload.

### iotsaUser.h

An authentication module (_IotsaAuthMod_) that stores a single username and password. Other modules can then specify they are only accessible after the user authenticates with this username/password combination.

Accessing URL `/users` allows changing the password, and if the password has never been set the default password is shown whenever the device is booted in configuration mode.
 
## sample programs
- [Skeleton](examples/Skeleton/Skeleton.ino) is a good starting point for your own applications.
- [Hello](examples/Hello/Hello.ino) is the simplest "Hello, user" server.
- [HelloCpp](examples/HelloCpp/HelloCpp.ino) is the same, but implemented using C++ class declarations.
- [Light](examples/Light/Light.ino) measures ambient light level with an LDR connected to the analog input.
- [Temperature](examples/Temperature/Temperature.ino) measures temperature with a slightly more complicated sensor, a DHT21.
- [Led](examples/Led/Led.ino) controls the color of a NeoPixel LED, and can set up repeating patterns. Uses _iotsaLed_ module.
- [SimpleIO](examples/SimpleIO/SimpleIO.ino) server that allows web access to analog and digital pins. Also shows how to use the _iotsaConfigFile_ classes.
- [HelloPasswd](examples/HelloPasswd/HelloPasswd.ino) The same "Hello" server, but now using a _IotsaAuthMod_ for access control (you need to provide username "admin" and password "admin" to change the greeting name).
- [HelloUser](examples/HelloUser/HelloUser.ino) Another "Hello" server that needs authentication, but this time using _IotsaUserMod_ so the password can be changed.
- [Log](examples/Log/Log.ino) Example of using the _iotsaLogger_ module.

## more projects using iotsa

Here are some projects that use iotsa, and that also be used as further examples (_Note that as of this writing not all projects may be publicly accessible yet_):

* [iotsaDisplayServer](http://github.com/cwi-dis/iotsaDisplayServer): Drives an LCD display, such as an i2c 4x40 character module. Support for a buzzer (to attract user attention) and buttons (programmable to trigger actions by accessing programmable URLs) is included.
* [iotsaMotorServer](http://github.com/cwi-dis/iotsaMotorServer): Drives one or more stepper motors. Schematics and 3D models are included for a device to lift an object (such as a plant in a pot) to a height that can be changed through the web.
* [iotsaNeoClock](http://github.com/cwi-dis/iotsaNeoClock): A clock comprised of 60 NeoPixel LEDs. Shows the time, but can also show programmable patterns (as alerts) and temporal information (such as expected rainfall for the coming hour). Schematics and building instructions included.
* [iotsaDoorOpener](http://github.com/cwi-dis/iotsaDoorOpener): Operates a solenoid to open a door. On web access, or when an RFID tag (such as a keychain fob or a mifare contactless transport card) is presented. RFID cards are programmable (over the net, or using a special "learn" card). A web request can be sent to a programmable URL when a card is presented.
* [iotsaSmartMeter](http://github.com/cwi-dis/iotsaSmartMeter): Reads electricity and gas usage of a dutch Smart Meter through the standardised P1 port and makes the data available on the net.

## hardware

Folder _extras/fritzing_ contains design (circuit and PCB, and partial breadboard layout)
for a PCB based on an ESP-12 that makes a nice iotsa hardware platform. You need the open source [Fritzing](http://fritzing.org/home/) tool to open these files.

The iotsa board has the ESP-12, a 3.3v regulator (so it can be powered with a standard 5v-16v power
supply) and a free section with enough space for a DIP IC (up to DIP-20) and/or
a few discrete components. All easily usable GPIO pins are also available.
There's also room for an FTDI header (so you can reprogram the board if you have bricked it with over-the-air programming) and _reset_ and _program_ buttons.

The v3 board is about 6.35x4.35cm in size, with the ESP-12 antenna sticking out
0.5cm.

### Case

Extras also has a file [extras/iotsaCase.scad](extras/iotsaCase.scad) that contains the OpenSCAD source code to
3D-print your own box to fit a iotsa board. Examine the source, there are various
ways to adjust the design (to add extra holes, or determine thickness of
the box, or make the box fit a iotsa board with some of the experimental area rows
removed.

The 3D-printable case for a iotsa board is also available at Thingyverse: <http://www.thingiverse.com/thing:2303793>.
