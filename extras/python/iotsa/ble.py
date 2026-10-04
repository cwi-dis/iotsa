import sys
import asyncio
import re
from typing import Any, Optional
from bleaktyped import BLEDevice, BleakClient, BleakGATTServiceCollection, BleakError, BleakScanner, BleakTypedClient
from .bleIotsaUUIDs import name_to_uuid, uuid_to_name
from .consts import VERBOSE

IOTSA_BATTERY_SERVICE = "0000180f-0000-1000-8000-00805f9b34fb"
# The runmode service is core/mandatory (IotsaRunmodeMod, cwi-dis/iotsa#106) --
# present on every iotsa device, unlike the battery service above, which is
# only there if the app happens to compile in IotsaBatteryMod (e.g.
# examples/BLERinger doesn't). Checked as well as, not instead of, the battery
# service so pre-#106 devices (battery service, no runmode service yet) are
# still discoverable.
IOTSA_RUNMODE_SERVICE = name_to_uuid("runmode")
IOTSA_REBOOT_CHARACTERISTIC = ""

class BLE:
    """Handle iotsa device connectable over Bluetooth LE"""

    discover_timeout = 11
    streamMTU = 500
    _currentDevice : Optional[BLEDevice]
    _currentConnection : Optional[BleakClient]
    _serviceCollection : Optional[BleakGATTServiceCollection]
    loop : asyncio.AbstractEventLoop

    def __init__(self):
        self.verbose = VERBOSE
        self._allDevices: list[str] = []
        self._currentDevice = None
        self._currentConnection = None
        self._serviceCollection = None
        self.loop = asyncio.new_event_loop()

    def __del__(self):
        if self._currentConnection:
            self.close()

    def close(self) -> None:
        if self._currentConnection != None:
            assert self._currentConnection
            self.loop.run_until_complete(self._currentConnection.disconnect())
            self._currentConnection = None
            
    def isConnected(self) -> bool:
        return self._currentConnection != None
    
    def findDevices(self) -> list[str]:
        try:
            self.loop.run_until_complete(self._asyncFindDevices())
        except BleakError as e:
            print(f"ble.findDevices: exception: {e}")
        return self._allDevices

    async def _asyncFindDevices(self):
        devices_and_advertisement_data_map = await BleakScanner.discover(timeout=self.discover_timeout, return_adv=True)
        # Every iotsa device advertises the runmode service (core, mandatory);
        # pre-#106 devices instead only have the battery service -- check for
        # either, so both old and new devices are found.
        iotsaCandidates = []
        for d, adv in devices_and_advertisement_data_map.values():
            if IOTSA_RUNMODE_SERVICE in adv.service_uuids or IOTSA_BATTERY_SERVICE in adv.service_uuids:
                iotsaCandidates.append(adv.local_name or d.name or d.address)
        if iotsaCandidates:
            self._allDevices = iotsaCandidates

    def selectDevice(self, name_or_address: str) -> bool:
        try:
            self.loop.run_until_complete(self._asyncSelectDevice(name_or_address))
        except BleakError as e:
            print(f"ble.selectDevice({name_or_address}, ...): exception: {e}")
        return self._currentDevice != None

    async def _asyncSelectDevice(self, name_or_address: str):
        if re.fullmatch("[0-9a-fA-F:-]*", name_or_address):
            dev = await BleakScanner.find_device_by_address(name_or_address)
        else:
            # Same name source as _asyncFindDevices() (bleTargets): the
            # advertised name first. d.name is the OS's cached device name,
            # which can be missing or stale (macOS), so a device listed by
            # bleTargets could not be selected by name.
            wanted = name_or_address.lower()
            dev = await BleakScanner.find_device_by_filter(
                lambda d, ad: (ad.local_name or d.name or "").lower() == wanted
            )
        if not dev:
            print(f"Device {name_or_address} not found")
            return
        self._currentDevice = dev
        client = BleakTypedClient(
            self._currentDevice, 
            timeout=self.discover_timeout
        )
        await client.connect()
        self._currentConnection = client

    def printStatus(self) -> None:
        try:
            self.loop.run_until_complete(self._asyncPrintStatus())
        except BleakError as e:
            print(f"ble.printsStatus: exception: {e}")

    async def _asyncPrintStatus(self):
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            print(f"{client.address}:")
            self._serviceCollection = client.services
            for service in self._serviceCollection.services.values():
                if self.verbose:
                    print(
                        f"{uuid_to_name(service.uuid)} ({service.uuid} {service.description}):"
                    )
                else:
                    print(f"  {uuid_to_name(service.uuid)}:")
                for char in service.characteristics:
                    if 'read' in char.properties:
                        try:
                            value = await client.read_gatt_char_typed(char.uuid)
                            if self.verbose:
                                print(
                                    f"    {uuid_to_name(char.uuid)}: {value} # uuid={char.uuid} description={char.description}"
                                )
                            else:
                                print(f"    {uuid_to_name(char.uuid)}: {value}")
                        except BleakError as e:
                            print(
                                f"    # {uuid_to_name(char.uuid)} cannot read, uuid={char.uuid} description={char.description} error={e}"
                            )
                        except AssertionError as e:
                            print(
                                f"    {uuid_to_name(char.uuid)} cannot read, uuid={char.uuid} description={char.description} error={e}"
                            )
                    else:
                        print(f"    # {uuid_to_name(char.uuid)} cannot read, only {','.join(char.properties)} allowed, uuid={char.uuid} description={char.description}")
                    sys.stdout.flush()

    def set(self, name: str, value: Any) -> None:
        try:
            self.loop.run_until_complete(self._asyncSet(name, value))
        except BleakError as e:
            print(f"ble.set({name}, ...): exception: {e}")

    async def _asyncSet(self, name: str, value: Any) -> None:
        uuid = name_to_uuid(name)
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            await client.write_gatt_char_typed(uuid, value, response=True)

    def setStreamed(self, name: str, value: bytes) -> None:
        # Unlike set()/get(), don't swallow BleakError here: this sends a value in
        # several chunks, and a failure partway through leaves the device holding a
        # partial value. Silently continuing (e.g. on to a controlPoint write) would
        # make the device process an incomplete body as if it were complete, so let
        # the exception propagate and abort the whole request instead.
        self.loop.run_until_complete(self._asyncSetStreamed(name, value))

    async def _asyncSetStreamed(self, name: str, value: bytes) -> None:
        uuid = name_to_uuid(name)
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            while len(value) > 0:
                currentBuffer = value[:self.streamMTU]
                value = value[self.streamMTU:]
                await client.write_gatt_char(uuid, currentBuffer, response=True)

    def get(self, name: str) -> Any:
        self._get_rv: Any = None
        try:
           self.loop.run_until_complete(self._asyncGet(name))
        except BleakError as e:
            print(f"ble.get({name}): exception: {e}")
        return self._get_rv

    async def _asyncGet(self, name: str):
        uuid = name_to_uuid(name)
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            try:
                self._get_rv = await client.read_gatt_char_typed(uuid)
            except BleakError as e:
                print(e)
                self._get_rv = None

    def getStreamed(self, name: str) -> Optional[bytes]:
        self._get_rv: Any = None
        try:
           self.loop.run_until_complete(self._asyncGetStreamed(name))
        except BleakError as e:
            print(f"ble.getStreamed({name}): exception: {e}")
        return self._get_rv

    async def _asyncGetStreamed(self, name: str):
        uuid = name_to_uuid(name)
        assert self._currentConnection
        rv = b""
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            while True:
                try:
                    currentBuffer = await client.read_gatt_char(uuid)
                except BleakError as e:
                    print(e)
                    self._get_rv = None
                    return
                if len(currentBuffer) == 0:
                    self._get_rv = rv
                    return
                rv = rv + currentBuffer



