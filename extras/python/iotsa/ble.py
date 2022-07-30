import sys
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
        self._currentConnection = None
        self.loop = asyncio.new_event_loop()

    def findDevices(self):
        self.loop.run_until_complete(self._asyncFindDevices())
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
        self.loop.run_until_complete(self._asyncSelectDevice(name_or_address))
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
        self._currentDevice = dev
        self._currentConnection = bleak.pythonic.client.BleakPythonicClient(self._currentDevice)
        print('xxxjack async select ended')

    def printStatus(self):
        print('xxxjack before async print status')
        
        self.loop.run_until_complete(self._asyncPrintStatus())
        print('xxxjack after async print status')

    async def _asyncPrintStatus(self):
        async with self._currentConnection as client:
            services = await client.get_services()
            for handle, char in services.characteristics.items():
                try:
                    value = await client.read_gatt_char_typed(char.uuid)
                    print(f'{handle}: {char.uuid} ({char.description}): {value}')
                except bleak.BleakError:
                    print(f'{handle}: {char.uuid} ({char.description}): cannot read')
                sys.stdout.flush()

    def set(self, name, value):
        self.loop.run_until_complete(self._asyncSet(name, value))

    async def _asyncSet(self, name, value):
        async with self._currentConnection as client:
            await client.write_gatt_char_typed(name, value, True)

    def get(self, name):
        self._get_rv = None
        self.loop.run_until_complete(self._asyncGet(name))
        return self._get_rv

    async def _asyncGet(self, name):
        async with self._currentConnection as client:
            try:
                self._get_rv = await client.read_gatt_char_typed(name)
            except bleak.BleakError as e:
                print(e)
                self._get_rv = None