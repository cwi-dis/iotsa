import sys
import json as jsonmod
from abc import ABC, abstractmethod
from typing import Any, List, Optional, Tuple, Type

# Framework metadata in API replies (cwi-dis/iotsa#280), the same over every transport.
API_ERROR_KEY = "iotsa_api_error"
API_IGNORED_KEY = "iotsa_api_ignored"

def apiErrorMessage(body: Any) -> Optional[str]:
    """The device's explanation of a failed request, if the error body has one."""
    if isinstance(body, (bytes, bytearray)):
        body = body.decode("utf-8", errors="replace")
    if isinstance(body, str):
        try:
            body = jsonmod.loads(body)
        except ValueError:
            return None
    if isinstance(body, dict):
        msg = body.get(API_ERROR_KEY)
        if isinstance(msg, str):
            return msg
    return None

def checkApiReply(reply: Any, what: str) -> Any:
    """Warn about fields the device ignored, and strip that metadata from the reply."""
    if isinstance(reply, dict) and API_IGNORED_KEY in reply:
        ignored = reply.pop(API_IGNORED_KEY)
        if ignored:
            print(f"{sys.argv[0]}: warning: {what}: device ignored {', '.join(map(str, ignored))}", file=sys.stderr)
    return reply

class IotsaAbstractProtocolHandler(ABC):
    """Abstract base class for REST and COAP protocol handlers"""

    @abstractmethod
    def __init__(
        self,
        baseURL: str,
        noverify: bool = False,
        bearer: Optional[str] = None,
        auth: Optional[Tuple[str, str]] = None,
    ):
        pass

    @abstractmethod
    def close(self):
        """Close the connection"""
        pass

    @abstractmethod
    def get(self, endpoint: str, json: Any = None) -> Any:
        """Send a REST GET request.

        :param endpoint: last part of URL
        :param json: optional argument, will be json-encoded
        :return: any return value, json-decoded
        """
        pass

    @abstractmethod
    def put(self, endpoint: str, json: Any = None) -> Any:
        """Send a REST PUT request.

        :param endpoint: last part of URL
        :param json: optional argument, will be json-encoded
        :return: any return value, json-decoded
        """
        pass

    @abstractmethod
    def post(
        self, endpoint: str, json: Any = None, files: Optional[dict[str, Any]] = None
    ) -> Any:
        """Send a REST POST request.

        :param endpoint: last part of URL
        :param json: optional argument, will be json-encoded
        :param files: optional files to upload, passed to requests.request
        :return: any return value, json-decoded
        """
        pass

    @abstractmethod
    def request(
        self,
        method: str,
        endpoint: str,
        json: Any = None,
        files: Optional[dict[str, Any]] = None,
        retryCount: int = 5,
    ) -> Any:
        """Send a REST request.

        :param method: REST method
        :param endpoint: last part of URL
        :param json: optional argument, will be json-encoded
        :param files: optional files to upload, passed to requests.request
        :return: any return value, json-decoded
        """
        pass

