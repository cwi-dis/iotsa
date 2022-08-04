import sys
import asyncio
import re
from typing import Any
import bleak
import bleak.pythonic.client
from .bleIotsaUUIDs import name_to_uuid, uuid_to_name

IOTSA_BATTERY_SERVICE = "0000180f-0000-1000-8000-00805f9b34fb"
IOTSA_REBOOT_CHARACTERISTIC = ""


class BLE:
    """Handle iotsa device connectable over Bluetooth LE"""

    discover_timeout = 11

    def __init__(self):
        self.verbose = False
        self._allDevices: list[str] = []
        self._currentDevice = None
        self._currentConnection = None
        self._serviceCollection = None
        self.loop = asyncio.new_event_loop()

    def findDevices(self) -> list[str]:
        self.loop.run_until_complete(self._asyncFindDevices())
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
        self.loop.run_until_complete(self._asyncSelectDevice(name_or_address))
        return self._currentDevice != None

    async def _asyncSelectDevice(self, name_or_address: str):
        if re.fullmatch("[0-9a-fA-F:-]*", name_or_address):
            dev = await bleak.BleakScanner.find_device_by_address(name_or_address)
        else:
            dev = await bleak.BleakScanner.find_device_by_filter(
                lambda d, ad: d.name and d.name.lower() == name_or_address
            )
        if not dev:
            print(f"Device {name_or_address} not found")
            return
        self._currentDevice = dev
        self._currentConnection = bleak.pythonic.client.BleakPythonicClient(
            self._currentDevice, timeout=self.discover_timeout
        )

    def printStatus(self) -> None:
        self.loop.run_until_complete(self._asyncPrintStatus())

    async def _asyncPrintStatus(self):
        async with self._currentConnection as client:
            self._serviceCollection = await client.get_services()
            for service in self._serviceCollection.services.values():
                if self.verbose:
                    print(
                        f"{uuid_to_name(service.uuid)} ({service.uuid} {service.description}):"
                    )
                else:
                    print(f"{uuid_to_name(service.uuid)}:")
                for char in service.characteristics:
                    try:
                        value = await client.read_gatt_char_typed(char.uuid)
                        if self.verbose:
                            print(
                                f"\t{uuid_to_name(char.uuid)}={value} ({char.uuid} {char.description})"
                            )
                        else:
                            print(f"\t{uuid_to_name(char.uuid)}={value}")
                    except bleak.BleakError as e:
                        print(
                            f"\t{uuid_to_name(char.uuid)} ({char.uuid} {char.description}): cannot read: {e}"
                        )
                    sys.stdout.flush()

    def set(self, name: str, value: Any) -> None:
        self.loop.run_until_complete(self._asyncSet(name, value))

    async def _asyncSet(self, name: str, value: Any) -> None:
        uuid = name_to_uuid(name)
        async with self._currentConnection as client:
            await client.write_gatt_char_typed(uuid, value, response=True)

    def get(self, name: str) -> Any:
        self._get_rv: Any = None
        self.loop.run_until_complete(self._asyncGet(name))
        return self._get_rv

    async def _asyncGet(self, name: str):
        uuid = name_to_uuid(name)
        async with self._currentConnection as client:
            try:
                self._get_rv = await client.read_gatt_char_typed(uuid)
            except bleak.BleakError as e:
                print(e)
                self._get_rv = None
