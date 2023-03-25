from typing import Type

from ..consts import UserIntervention, IotsaError, CoapError, VERBOSE
from .abstract import IotsaAbstractProtocolHandler
from .http import IotsaRESTProtocolHandler
from .coap import IotsaCOAPProtocolHandler


HandlerForProto: dict[str, Type[IotsaAbstractProtocolHandler]] = {
    "http": IotsaRESTProtocolHandler,
    "https": IotsaRESTProtocolHandler,
    "coap": IotsaCOAPProtocolHandler,
}
