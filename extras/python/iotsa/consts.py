from typing import List

VERBOSE: List[bool] = []


class UserIntervention(Exception):
    """Operation requires user intervention"""

    pass


class IotsaError(RuntimeError):
    """General error from iotsa package"""

    pass


class CoapError(IotsaError):
    """COAP error from iotsa package"""

    pass

class HpsError(IotsaError):
    """HPS error from iotsa package"""

    pass