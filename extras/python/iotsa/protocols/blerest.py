import socket
import urllib.parse
import urllib.request
import json as jsonmod

from .abstract import IotsaAbstractProtocolHandler
from ..consts import VERBOSE, IotsaError
from ..ble import BLE

class IotsaBLERESTProtocolHandler(IotsaAbstractProtocolHandler):
    """Communicate with iotsa device using REST over BLE

    :param baseurl: first part of URL (endpoint arguments will be appended)
    """

    def __init__(self, baseURL: str, bearer=None, noverify=None, auth=None):
        if VERBOSE:
            print(f"IotsaBLERestProtocolHandler({baseURL})")
        self.client = None
        if bearer:
            raise IotsaError("bearer not supported for blerest")
        if auth:
            raise IotsaError("auth not supported for blerest")
        parts = urllib.parse.urlparse(baseURL)
        self.basePath = parts.path
        if not self.basePath:
            self.basePath = "/api/"
        assert not parts.params
        assert not parts.query
        assert not parts.fragment
        assert parts.netloc
        assert not ":" in parts.netloc
        self.bleServer = parts.netloc
        self.client = BLE()

        pass # self.client = coapthon.client.helperclient.HelperClient(server=(host, port))

    def __del__(self):
        self.close()

    def close(self):
        if self.client:
            self.client.close()
        self.client = None
    
    METHOD_TO_CODE = {
        "GET" : 0x01,
        "POST" : 0x03,
        "PUT" : 0x04
    }
    def request(self, method, endpoint, json=None, files=None, retryCount=5):
        endpoint = self.basePath + endpoint
        headers = ""
        data = jsonmod.dumps(json)
        if VERBOSE:
            print(f"BLEREST {method} blerest://{self.bleServer}{endpoint}")
        if not self.client.isConnected():
            self.client.selectDevice(self.bleServer)
        self.client.set("hpsURL", endpoint)
        self.client.set("hpsHeaders", headers)
        if json != None:
            data = jsonmod.dumps(json)
            self.client.set("hpsBody", data.encode())
        commandCode = self.METHOD_TO_CODE[method]
        self.client.set("hpsControlPoint", commandCode)

        fullStatus = self.client.get("hpsStatus")
        if VERBOSE:
            print(f"BLEREST status {repr(fullStatus)}")
        rvBytes = self.client.get("hpsBody")
        if rvBytes == None or len(rvBytes) == 0:
            if VERBOSE:
                print(f"BLEREST {method} returned empty response")
            return None
        if VERBOSE:
            print(f"BLEREST {method} returned {rvBytes}")
        return jsonmod.loads(rvBytes)

    def get(self, endpoint, json=None):
        return self.request("GET", endpoint, json=json)

    def put(self, endpoint, json=None):
        return self.request("PUT", endpoint, json=json)

    def post(self, endpoint, json=None, files=None):
        assert files is None or json is None
        return self.request("POST", endpoint, json=json, files=files)
