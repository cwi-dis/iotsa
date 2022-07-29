import asyncio
import re
import bleak
import bleak.pythonic.client

IOTSA_BATTERY_SERVICE="0000180f-0000-1000-8000-00805f9b34fb"
IOTSA_REBOOT_CHARACTERISTIC=""

class BLE:
    def __init__(self):
        self._allDevices = None
        self._currentDevice = None

    def findDevices(self):
        asyncio.run(self._asyncFindDevices())
        return self._allDevices

    async def _asyncFindDevices(self):
        candidates = await bleak.discover()
        # Iotsa devices have a battery service, and a reboot charcteristic in that service.
        # So filter for those.
        iotsaCandidates = []
        for d in candidates:
            if IOTSA_BATTERY_SERVICE in d.metadata.get('uuids', []):
                print(f"xxxjack device={d} uuids={d.metadata.get('uuids')} mfgdata={d.metadata.get('manufacturer_data')}")
                iotsaCandidates.append(d)
        if iotsaCandidates:
            self._allDevices = iotsaCandidates

    def selectDevice(self, name_or_address):
        print('xxxjack before async select')
        asyncio.run(self._asyncSelectDevice(name_or_address))
        print('xxxjack after async select')
        return self._currentDevice != None

    async def _asyncSelectDevice(self, name_or_address):
        print('xxxjack async select started')
        if re.fullmatch('[0-9a-fA-F:-]*', name_or_address):
            dev = await bleak.BleakScanner.find_device_by_address(name_or_address)
        else:
            dev = await bleak.BleakScanner.find_device_by_filter(
                lambda d, ad: d.name and d.name.lower() == name_or_address
            )
        if not dev:
            print(f'Device {name_or_address} not found')
            return
        self._currentDevice = bleak.pythonic.client.BleakPythonicClient(dev)
        print('xxxjack async select ended')

    def printStatus(self):
        print('xxxjack before async print status')
        asyncio.run(self._asyncPrintStatus())
        print('xxxjack after async print status')

    async def _asyncPrintStatus(self):
        services = await self._currentDevice.get_services()
        print(f"services {services}")

    def set(self, name, value):
        asyncio.run(self._asyncSet(name, value))

    async def _asyncSet(self, name, value):
        await self._currentDevice.write_gatt_char_typed(name, value, True)

    def get(self, name):
        self._get_rv = None
        asyncio.run(self._asyncGet(name))
        return self._get_rv

    async def _asyncGet(self, name):
        self._get_rv = await self._currentDevice.read_gatt_char_typed(name)