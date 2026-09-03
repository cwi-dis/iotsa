from .consts import IotsaError

_uuid_to_name: dict[str, str] = {
    "0000180f-0000-1000-8000-00805f9b34fb": "battery",
    "00002a19-0000-1000-8000-00805f9b34fb": "levelBattery",
    "e4d90002-250f-46e6-90a4-ab98f01a0587": "levelVUSB",

    # The iotsa runmode control service (cwi-dis/iotsa#106). Replaces the old
    # "reboot with WiFi" characteristic that lived on the battery service.
    "6e5d0001-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "runmode",
    "6e5d0002-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "currentMode",
    "6e5d0003-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "requestedMode",
    "6e5d0004-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "reboot",
    "6e5d0005-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "promoteMode",
    "6e5d0006-f2a7-4e7a-9b1c-2d3e4f5a6b7c": "wifiDisabled",

    "3b000001-1226-4a53-9d24-afa50c0163a3": "led",
    "3b000002-1226-4a53-9d24-afa50c0163a3": "rgb",

    "6b2f0001-38bc-4204-a506-1d3546ad3688": "lissabon",
    "6b2f0002-38bc-4204-a506-1d3546ad3688": "isOn",
    "6b2f0003-38bc-4204-a506-1d3546ad3688": "identify",
    "6b2f0004-38bc-4204-a506-1d3546ad3688": "brightness",
    "6b2f0005-38bc-4204-a506-1d3546ad3688": "temperature",

    "00001823-0000-1000-8000-00805f9b34fb": "hps",
    "00002ab6-0000-1000-8000-00805f9b34fb" : "hpsURL",
    "00002ab7-0000-1000-8000-00805f9b34fb" : "hpsHeaders",
    "00002ab8-0000-1000-8000-00805f9b34fb" : "hpsStatus",
    "00002ab9-0000-1000-8000-00805f9b34fb" : "hpsBody",
    "00002aba-0000-1000-8000-00805f9b34fb" : "hpsControlPoint",
    "00002abb-0000-1000-8000-00805f9b34fb" : "hpsSecurity",
}

_name_to_uuid: dict[str, str] = {v: k for k, v in _uuid_to_name.items()}


def uuid_to_name(uuid: str) -> str:
    uuid = uuid.lower()
    return _uuid_to_name.get(uuid, uuid)


def name_to_uuid(name: str) -> str:
    if name in _name_to_uuid:
        return _name_to_uuid[name]
    if len(name) == 32 + 4:
        return name
    raise IotsaError(f"Unknown BLE characteristic name: {name}")
