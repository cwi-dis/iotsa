# iotsaControl

IotsaControl is a program (and a Python module) that allows progammatic control over [Iotsa](https://github.com/cwi-dis/iotsa) devices. The source code lives in the Iotsa repository (in _extras/python_).

Iotsa devices are small internet-based IoT appliances. Like IotsaControl, Iotsa is open source and an open hardware platform.

iotsaControl allows getting sensor readings and setting actuators through shell scripts and Python programs.
It also allows you to discover all the Iotsa devices on the local network, and all fresh (uninitialized) Iotsa devices within WiFi range. And it allows you to configure those devices.

No documentation is available yet, but for command line usage type

```
iotsaControl help
iotsaControl --help
```
For programmatic use inspect the `iotaControl.api` module.
