import socket
import urllib.parse
import urllib.request
import json as jsonmod
import coapthon.client.helperclient
import coapthon.defines

from .abstract import IotsaAbstractProtocolHandler
from ..consts import CoapError, VERBOSE

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

    def request(self, method, endpoint, json=None, files=None, retryCount=5):
        assert False, "Only get/put/post implemented for COAP, not request()"
