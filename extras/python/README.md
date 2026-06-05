# iotsa (Python tool)

`iotsa` is a program (and a Python module) that allows programmatic control over [Iotsa](https://github.com/cwi-dis/iotsa) devices. The source code lives in the Iotsa repository (in _extras/python_).

Iotsa devices are small internet-based IoT appliances. Like the iotsa framework, this tool is open source.

The tool allows getting sensor readings and setting actuators through shell scripts and Python programs.
It also allows you to discover all the Iotsa devices on the local network, and all fresh (uninitialized) Iotsa devices within WiFi range. And it allows you to configure those devices.

No documentation is available yet, but for command line usage type

```
iotsa help
iotsa --help
```
For programmatic use inspect the `iotsa.api` module.
