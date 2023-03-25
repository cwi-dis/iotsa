from abc import ABC, abstractmethod
from typing import Any, List, Optional, Tuple, Type

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

