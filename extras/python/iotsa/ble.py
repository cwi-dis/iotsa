import sys
import asyncio
import re
from typing import Any, Optional
import bleak
import bleak.pythonic.client
from .bleIotsaUUIDs import name_to_uuid, uuid_to_name

IOTSA_BATTERY_SERVICE = "0000180f-0000-1000-8000-00805f9b34fb"
IOTSA_REBOOT_CHARACTERISTIC = ""

class BLE:
    """Handle iotsa device connectable over Bluetooth LE"""

    discover_timeout = 11
    streamMTU = 500
    _currentDevice : Optional[bleak.BLEDevice]
    _currentConnection : Optional[bleak.BleakClient]
    _serviceCollection : Optional[bleak.BleakGATTServiceCollection]
    loop : asyncio.AbstractEventLoop

    def __init__(self):
        self.verbose = False
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
            self.loop.run_until_complete(self._currentConnection.disconnect())
            self._currentConnection = None
            
    def isConnected(self) -> bool:
        return self._currentConnection != None
    
    def findDevices(self) -> list[str]:
        try:
            self.loop.run_until_complete(self._asyncFindDevices())
        except bleak.BleakError as e:
            print(f"ble.findDevices: exception: {e}")
        return self._allDevices

    async def _asyncFindDevices(self):
        candidates = await bleak.BleakScanner.discover(timeout=self.discover_timeout)
        # Iotsa devices have a battery service, and a reboot charcteristic in that service.
        # So filter for those.
        iotsaCandidates = []
        for d in candidates:
            if IOTSA_BATTERY_SERVICE in d.metadata.get("uuids", []):
                iotsaCandidates.append(d)
        if iotsaCandidates:
            self._allDevices = iotsaCandidates

    def selectDevice(self, name_or_address: str) -> bool:
        try:
            self.loop.run_until_complete(self._asyncSelectDevice(name_or_address))
        except bleak.BleakError as e:
            print(f"ble.set({name_or_address}, ...): exception: {e}")
        return self._currentDevice != None

    async def _asyncSelectDevice(self, name_or_address: str):
        if re.fullmatch("[0-9a-fA-F:-]*", name_or_address):
            dev = await bleak.BleakScanner.find_device_by_address(name_or_address)
        else:
            dev = await bleak.BleakScanner.find_device_by_filter(
                lambda d, ad: bool(d.name) and d.name.lower() == name_or_address
            )
        if not dev:
            print(f"Device {name_or_address} not found")
            return
        self._currentDevice = dev
        client = bleak.pythonic.client.BleakPythonicClient(
            self._currentDevice, 
            timeout=self.discover_timeout
        )
        await client.connect()
        self._currentConnection = client

    def printStatus(self) -> None:
        try:
            self.loop.run_until_complete(self._asyncPrintStatus())
        except bleak.BleakError as e:
            print(f"ble.printsStatus: exception: {e}")

    async def _asyncPrintStatus(self):
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            print(f"{client.address}:")
            self._serviceCollection = await client.get_services()
            for service in self._serviceCollection.services.values():
                if self.verbose:
                    print(
                        f"{uuid_to_name(service.uuid)} ({service.uuid} {service.description}):"
                    )
                else:
                    print(f"  {uuid_to_name(service.uuid)}:")
                for char in service.characteristics:
                    try:
                        value = await client.read_gatt_char_typed(char.uuid)
                        if self.verbose:
                            print(
                                f"    {uuid_to_name(char.uuid)}: {value} # uuid={char.uuid} description={char.description}"
                            )
                        else:
                            print(f"    {uuid_to_name(char.uuid)}: {value}")
                    except bleak.BleakError as e:
                        print(
                            f"    {uuid_to_name(char.uuid)} cannot read, uuid={char.uuid} description={char.description} error={e}"
                        )
                    sys.stdout.flush()

    def set(self, name: str, value: Any) -> None:
        try:
            self.loop.run_until_complete(self._asyncSet(name, value))
        except bleak.BleakError as e:
            print(f"ble.set({name}, ...): exception: {e}")

    async def _asyncSet(self, name: str, value: Any) -> None:
        uuid = name_to_uuid(name)
        assert self._currentConnection
        # async with self._currentConnection as client:
        if True:
            client = self._currentConnection
            await client.write_gatt_char_typed(uuid, value, response=True)

    def setStreamed(self, name: str, value: bytes) -> None:
        try:
            self.loop.run_until_complete(self._asyncSetStreamed(name, value))
        except bleak.BleakError as e:
            print(f"ble.setStreamed({name}, ...): exception: {e}")

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
        except bleak.BleakError as e:
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
            except bleak.BleakError as e:
                print(e)
                self._get_rv = None

    def getStreamed(self, name: str) -> Optional[bytes]:
        self._get_rv: Any = None
        try:
           self.loop.run_until_complete(self._asyncGetStreamed(name))
        except bleak.BleakError as e:
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
                except bleak.BleakError as e:
                    print(e)
                    self._get_rv = None
                    return
                if len(currentBuffer) == 0:
                    self._get_rv = rv
                    return
                rv = rv + currentBuffer



