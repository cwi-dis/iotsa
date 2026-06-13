# iotsa (Python tool)

`iotsa` is a command-line tool (and Python module) for discovering, inspecting, configuring, and updating [iotsa](https://github.com/cwi-dis/iotsa) devices over the network.

## Setup

From any iotsa application repo root (or the iotsa repo itself):

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -e ../iotsa/extras/python/
```

Or install non-editable:

```sh
pip install extras/python/
```

## Common usage

All commands take `--target` (or `-t`) to specify the device. You can use a hostname or IP address.

### Discover devices on the network

```sh
iotsa targets
```

### Get device information

```sh
iotsa -t yourdevice.local info
iotsa -t yourdevice.local allInfo
iotsa -t yourdevice.local version
```

### Get or set a specific module's configuration

```sh
iotsa -t yourdevice.local xInfo modulename
iotsa -t yourdevice.local xConfig modulename key=value
```

Parameters are `name=value`. Values are auto-coerced: `true`/`false` become booleans, anything that looks like an integer becomes an int, then a float, otherwise it stays a string. Use `name=type:value` to override (e.g. `name=str:123` to pass a number as a string).

### WiFi information and configuration

```sh
iotsa -t yourdevice.local wifiInfo
iotsa -t yourdevice.local wifiConfig ssid=mynetwork ssidpw=mypassword
```

WiFi config requires the device to be in configuration or private WiFi mode first.

### OTA firmware update

The typical workflow — put the device in OTA mode, then upload:

```sh
iotsa -t yourdevice.local otaWait ota path/to/firmware.bin
```

`otaWait` asks the device to enter OTA mode and waits until it does (you may need to power-cycle). `ota` then uploads the firmware binary.

### Configuration mode

To change settings that require configuration mode (e.g. WiFi, device name):

```sh
iotsa -t yourdevice.local configWait config name=value
```

### Reboot and factory reset

```sh
iotsa -t yourdevice.local reboot
iotsa -t yourdevice.local factoryReset
```

### DFU (wired USB/serial) programming

```sh
iotsa dfuMode
iotsa dfuLoad path/to/firmware.bin
iotsa dfuClear
```

DFU commands operate on a locally connected device, no `--target` needed.

## Authentication

If the device requires authentication:

```sh
iotsa -t yourdevice.local --credentials user:password info
iotsa -t yourdevice.local --bearer TOKEN info
iotsa -t yourdevice.local --access TOKEN info
```

## HTTPS

```sh
iotsa -t yourdevice.local --protocol https --noverify info
```

`--noverify` disables certificate verification, needed for self-signed certs.

## BLE

```sh
iotsa bleTargets
iotsa bleInfo
iotsa ble characteristicUUID value
```

## All options

```
iotsa --help
```
