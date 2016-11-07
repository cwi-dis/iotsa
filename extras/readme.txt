The "fritzing" subdirectory contains various files that describe
hardware that can be used with iotsa. Most of the files are intended for
use with the open source Fritzing design tool, see http://fritzing.org
to download and for details.

Here's what you can do with the various files:

iotsa-pcb-esp12.fzz 
	This is a Fritzing sketch that has the design for a iotsa
	board consisting of the ESP-12, a 3.3v power supply, and experimental
	area and some buttons (reset, program) and an FTDI connector. The
	breadboard view shows you how to wire things up so you can experiment
	with your own circuits. You can also add your own components to the
	breadboard, to document the circuit that you are using to interface to
	whatever hardware you connect your iotsa server to.

	The PCB view can be sent to Fritzing Fab to get some PCBs made (or, when
	exported to Gerber or something, to have someone else make the PCBs).

iotsa esp-12 experiment board.fzpz
	This is similar, but different. This is a Fritzing part, which means you can
	use it in the Fritzing Breadboard view as the basis (similarly to how you
	would normally use a breadboard), and add the components in their right places.
	There is no useful circuit or PCB view for this part.
	
	A course of action I follow is to first duplicate the sketch and there document
	my hardware as I am trying it out on a breadboard. Then, when it works, I
	re-layout the same components in a new sketch, based on the iotsa part,
	to document how I am putting the components on the experiment PCB.
	
Bare-ESP-12F.fzpz
	A Fritzing part for the bare ESP-12 (in PCB view) or the ESP-12 with a minimal
	carrier board (in BB view). Used by iotsa-pcb-esp12.fzz and provided here
	for your enjoyment (i.e. I don't think you need it).
	
L78L33.fzz
	A Fritzing part for the L78L33 power regulator. Actually, this regulator is
	not used anymore in the current version of the iotsa board because it turned
	out that it didn't supply enough current to power the ESP-12 under all
	circumstances. Again, provided for your enjoyment.
	