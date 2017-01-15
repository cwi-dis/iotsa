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