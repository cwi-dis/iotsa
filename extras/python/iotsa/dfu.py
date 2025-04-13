import os
import esptool
import urllib.request
from typing import Optional, List

from .consts import IotsaError, VERBOSE


class DFU:
    """Handle iotsa board connected to USB or serial port in DFU mode.

    Resetting flash and uploading initial binary is supported.
    Board should be rebooted while holding PRG (gpio0) button.
    """

    def __init__(self, port: Optional[str] = None):
        self.port = port
        self.verbose = VERBOSE

    def _run(self, cmd: str, args: list[str] = [], nostub=False) -> None:
        """Run a single esptool command"""
        # command = ["--after", "no_reset"]
        command = []
        if self.port:
            command += ["--port", self.port]
        if nostub:
            command += ["--no-stub"]
        command += [cmd]
        command += args
        if self.verbose:
            print(f"+ esptool {' '.join(command)}")
        try:
            esptool.main(command)
        except esptool.FatalError as e:
            raise IotsaError(str(e))

    def dfuWait(self) -> None:
        """Test to see whether a board in programmable mode is connected.

        Raises exception if not.
        """
        try:
            self._run("chip_id", nostub=True)
        except IotsaError:
            print("** To use DFU mode on a iotsa device:")
            print(
                "   1. Connect iotsa device to serial using FTDI232 or similar interface"
            )
            print("   2. Ensure iotsa device is powered through normal power supply")
            print("   3. Press-and-hold PRG button while pressing RST button")
            raise

    def dfuClear(self) -> None:
        """Erase flash."""
        self.dfuWait()
        self._run("erase_flash")

    def dfuRun(self) -> None:
        """Run program stored in iotsa flash memory"""
        self.dfuWait()
        self._run("run", nostub=True)

    def dfuLoad(self, filename: str) -> None:
        """Load a new program into iotsa flash memory"""
        self.dfuWait()
        if not os.path.exists(filename):
            filename, _ = urllib.request.urlretrieve(filename)
        self._run("write_flash", args=["0", filename])

    def dfuTool(self, args : List[str]):
        self._run(args[0], args[1:])