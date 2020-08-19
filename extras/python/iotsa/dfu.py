from __future__ import print_function
from __future__ import absolute_import
from __future__ import unicode_literals
from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
import os
import esptool
import urllib.request

from .consts import IotsaError, VERBOSE

class DFU(object):
    """Handle Resetting flash and uploading initial binary over Serial connection (or USB)"""
    def __init__(self, port=None):
        self.port = port
        
    def _run(self, cmd, args=[], nostub=False):
        """Run a single esptool command"""
        command = ["--after", "no_reset"]
        if self.port:
            command += ["--port", self.port]
        if nostub:
            command += ["--no-stub"]
        command += [cmd]
        command += args
        try:
            esptool.main(command)
        except esptool.FatalError as e:
            raise IotsaError(str(e))
            
    def dfuWait(self):
        """Test to see whether a board in programmable mode is connected"""
        try:
            self._run("chip_id", nostub=True)
        except IotsaError:
            print("** To use DFU mode on a iotsa device:")
            print("   1. Connect iotsa device to serial using FTDI232 or similar interface")
            print("   2. Ensure iotsa device is powered through normal power supply")
            print("   3. Press-and-hold PRG button while pressing RST button")
            raise
        
    def dfuClear(self):
        """Erase flash"""
        self.dfuWait()
        self._run("erase_flash")
        
    def dfuRun(self):
        """Run program stored in iotsa flash memory"""
        self.dfuWait()
        self._run("run", nostub=True)
        
    def dfuLoad(self, filename):
        """Load a new program into iotsa flash memory"""
        self.dfuWait()
        if not os.path.exists(filename):
            filename, _ = urllib.request.urlretrieve(filename)
        self._run("write_flash", args=["0", filename])
