import sys
import socket
import time
from abc import ABC, abstractmethod
from typing import Any, List, Optional, Tuple, Type
import urllib.parse
import urllib.request
import json as jsonmod
import coapthon.client.helperclient
import coapthon.defines
import requests

# We disable warnings about invalid certificates: we expect them and they provide no meaningful information
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

from .consts import UserIntervention, IotsaError, CoapError, VERBOSE


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


class IotsaRESTProtocolHandler(IotsaAbstractProtocolHandler):
    """Communicate with iotsa device using REST over HTTP or HTTPS

    :param baseurl: first part of URL (endpoint arguments will be appended)
    :param noverify: skip SSL certificate verification (debugging only)
    :param bearer: (optional) Authorization bearer token
    :param auth: (optional) Anthentication tuple
    """

    def __init__(
        self,
        baseURL: str,
        noverify: bool = False,
        bearer: Optional[str] = None,
        auth: Optional[Tuple[str, str]] = None,
    ):
        if baseURL[-1] != "/":
            baseURL += "/"
        self.baseURL = baseURL
        self.noverify = noverify
        self.bearer = bearer
        self.auth = auth

    def close(self):
        pass

    def get(self, endpoint, json=None):
        return self.request("GET", endpoint, json=json)

    def put(self, endpoint, json=None):
        return self.request("PUT", endpoint, json=json)

    def post(self, endpoint, json=None, files=None):
        assert files is None or json is None
        return self.request("POST", endpoint, json=json, files=files)

    def request(self, method, endpoint, json=None, files=None, retryCount=5):
        headers = {}
        if self.bearer:
            headers["Authorization"] = "Bearer " + self.bearer
        if endpoint[:1] == "/":
            url = self.baseURL + endpoint[1:]
        else:
            url = self.baseURL + "api/" + endpoint
        if VERBOSE:
            print("REST %s %s" % (method, url))
            if self.auth:
                print("auth", self.auth)
            if headers:
                print("....", headers)
            if json:
                print(">>>>", json)
        while True:
            try:
                r = requests.request(
                    method,
                    url,
                    auth=self.auth,
                    json=json,
                    verify=not self.noverify,
                    headers=headers,
                    files=files,
                )
            except requests.exceptions.SSLError:
                raise
            except requests.exceptions.ConnectionError:
                if retryCount > 0:
                    retryCount -= 1
                    time.sleep(2)
                else:
                    raise
            else:
                break
        if r.history:
            print("Note: received redirect when accessing", url)
        if VERBOSE:
            print("<<<< status=%s reply=%s" % (r.status_code, r.text))
        r.raise_for_status()
        if r.text and r.text[0] == "{":
            return r.json()
        return None


class IotsaCOAPProtocolHandler(IotsaAbstractProtocolHandler):
    """Communicate with iotsa device using COAP over UDP

    :param baseurl: first part of URL (endpoint arguments will be appended)
    """

    def __init__(self, baseURL: str, bearer=None, noverify=None, auth=None):
        self.client = None
        if bearer:
            raise CoapError("bearer not supported for coap")
        if auth:
            raise CoapError("auth not supported for coap")
        parts = urllib.parse.urlparse(baseURL)
        self.basePath = parts.path
        if not self.basePath:
            self.basePath = "/"
        assert not parts.params
        assert not parts.query
        assert not parts.fragment
        assert parts.netloc
        if ":" in parts.netloc:
            host, _port = parts.netloc.split(":")
            port = int(_port)
        else:
            host = parts.netloc
            port = 5683
        try:
            host = socket.gethostbyname(host)
        except socket.gaierror:
            pass
        self.client = coapthon.client.helperclient.HelperClient(server=(host, port))

    def __del__(self):
        self.close()

    def close(self):
        if self.client:
            self.client.stop()
        self.client = None

    def _raiseIfError(self, reply):
        if not reply:
            raise CoapError("reply is None")
        if reply.code >= 128:
            codeMajor = (reply.code >> 5) & 0x07
            codeMinor = reply.code & 0x1F
            codeName = coapthon.defines.Codes.LIST.get(reply.code, None)
            if codeName:
                codeName = codeName.name
            else:
                codeName = "unknown"
            raise CoapError("%d.%02d %s" % (codeMajor, codeMinor, codeName))

    def get(self, endpoint, json=None):
        assert json is None
        endpoint = self.basePath + endpoint
        if VERBOSE:
            print(
                "COAP GET coap://%s:%d%s"
                % (self.client.server[0], self.client.server[1], endpoint)
            )
        rv = self.client.get(endpoint)
        self._raiseIfError(rv)
        if VERBOSE:
            print("COAP GET returned", rv.code, len(rv.payload), repr(rv.payload))
        return jsonmod.loads(rv.payload)

    def put(self, endpoint, json=None):
        assert json is not None
        endpoint = self.basePath + endpoint
        data = jsonmod.dumps(json)
        if VERBOSE:
            print(
                "COAP PUT coap://%s:%d%s %s"
                % (self.client.server[0], self.client.server[1], endpoint, data)
            )
        rv = self.client.put(
            endpoint, (coapthon.defines.Content_types["application/json"], data)
        )
        self._raiseIfError(rv)
        if VERBOSE:
            print("COAP PUT returned", rv.code, repr(rv.payload))
        return jsonmod.loads(rv.payload)

    def post(self, endpoint, json=None, files=None):
        assert json is not None
        assert files is None
        endpoint = self.basePath + endpoint
        data = jsonmod.dumps(json)
        if VERBOSE:
            print(
                "COAP POST coap://%s:%d%s"
                % (self.client.server[0], self.client.server[1], endpoint, data)
            )
        rv = self.client.post(
            endpoint, (coapthon.defines.Content_types["application/json"], data)
        )
        self._raiseIfError(rv)
        if VERBOSE:
            print("COAP POST returned", rv.code, repr(rv.payload))
        return jsonmod.loads(rv.payload)


HandlerForProto: dict[str, Type[IotsaAbstractProtocolHandler]] = {
    "http": IotsaRESTProtocolHandler,
    "https": IotsaRESTProtocolHandler,
    "coap": IotsaCOAPProtocolHandler,
}
