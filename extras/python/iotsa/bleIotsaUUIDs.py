from .consts import IotsaError

_uuid_to_name: dict[str, str] = {
    "0000180f-0000-1000-8000-00805f9b34fb": "battery",
    "00002a19-0000-1000-8000-00805f9b34fb": "levelBattery",
    "e4d90002-250f-46e6-90a4-ab98f01a0587": "levelVUSB",
    "e4d90003-250f-46e6-90a4-ab98f01a0587": "rebootWifi",
    "3b000001-1226-4a53-9d24-afa50c0163a3": "led",
    "3b000002-1226-4a53-9d24-afa50c0163a3": "rgb",
    "6b2f0001-38bc-4204-a506-1d3546ad3688": "lissabon",
    "6b2f0002-38bc-4204-a506-1d3546ad3688": "isOn",
    "6b2f0003-38bc-4204-a506-1d3546ad3688": "identify",
    "6b2f0004-38bc-4204-a506-1d3546ad3688": "brightness",
    "6b2f0005-38bc-4204-a506-1d3546ad3688": "temperature",
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
