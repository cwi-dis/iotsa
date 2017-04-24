# iotsa - Internet of Things server architecture

This library contains a framework to easily create esp8266-based web servers. At the moment the servers can be partially REST-compatible, more support for this will be added later.

PCB design for a small board with an ESP-12 and room for additional hardware is included, see below.

## general design philosophy

Unlike most Arduino libraries and frameworks Iotsa does expose some of its C++ interfaces, but for simple applications you do not have to worry about this.

Iotsa has two main types of objects:

* `IotsaApplication`, of which there is only one, which is the web server and the container for the plugin modules.
* `IotsaMod` which is a plugin module and of which there can be many. Each plugin module provides a web interface (usually with a HTML form to allow control over it) plus some functionality. The `IotsaMod` class is subclassed to provide specific functionality. 

  One subclass that is always used is the `IotsaWifiMod`, which provides the functionality to connect to a specific Wifi network (after the user has provided the name and password). 
  
  One that is often used is `IotsaSimpleMod` which allows you to write two functions to implement your own functionality (your reason for actually using Iotsa).

### Do-nothing application

You create a global variable `app` of type `IotsaApplication` to hold the basic implementation of your service framework, plus the `ESP8266WebServer` object on which the application will serve. You also create one `IotsaWifiMod` and link it to the application so the end user can configure the WiFi network to join and such.

In your `setup()` function you call `app.setup()` and `app.serverSetup()` which will initialize the Iotsa framework.

In your `loop()` function you call `app.loop()`. This will take care of handling requests and every thing else that is needed to make the framework work.

You have now created a Iotsa server that does absolutely nothing. The _Skeleton_ example does exactly this (if you turn the various `#define` into `#undef`).

### Hello world application

_to be provided_

## available modules

_to be provided_

## sample programs

_to be provided_

## more projects using iotsa

_to be provided_

## hardware

Folder extras/fritzing contains design (circuit and PCB, and partial breadboard layout)
for a PCB based on an ESP-12 that makes a nice iotsa hardware platform.

It has the ESP-12, a 3.3v regulator (so it can be powered with a standard 5v power
supply) and a free section with enough space for a DIP IC (up to DIP-20) and/or
a few discrete components. All easily usable GPIO pins are also available.
There's also room for an FTDI header and reset/program buttons.

The v3 board is about 6.35x4.35cm in size, with the ESP-12 antenna sticking out
0.5cm.

Extras also has a file iotsaCase.scad that contains the OpenSCAD source code to
3D-print your own box to fit a iotsa board. Examine the source, there are various
ways to adjust the design (to add extra holes, or determine thickness of
the box, or make the box fit a iotsa board with some of the experimental area rows
removed.