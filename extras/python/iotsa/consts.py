from typing import List

VERBOSE: List[bool] = []


class UserIntervention(Exception):
    """Operation requires user intervention"""

    pass


class IotsaError(RuntimeError):
    """General error from iotsa package"""

    pass


class CoapError(Exception):
    """COAP error from iotsa package"""

    pass
