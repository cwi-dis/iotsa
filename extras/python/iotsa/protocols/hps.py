import sys
import socket
import struct
import urllib.parse
import urllib.request
import json as jsonmod

from .abstract import IotsaAbstractProtocolHandler
from ..consts import VERBOSE, IotsaError, HpsError
from ..ble import BLE

class IotsaHPSProtocolHandler(IotsaAbstractProtocolHandler):
    """Communicate with iotsa device using REST over BLE HTTP Proxy Service

    :param baseurl: first part of URL (endpoint arguments will be appended)
    """

    def __init__(self, baseURL: str, bearer=None, noverify=None, auth=None):
        if VERBOSE:
            print(f"IotsaHPSProtocolHandler({baseURL})")
        self.client = None
        if bearer:
            raise IotsaError("bearer not supported for hps")
        if auth:
            raise IotsaError("auth not supported for hps")
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
            print(f"HPS {method} hps://{self.bleServer}{endpoint}")
        if not self.client.isConnected():
            ok = self.client.selectDevice(self.bleServer)
            if not ok:
                raise HpsError(f"Cannot connect to {self.bleServer}")
        self.client.set("hpsURL", endpoint)
        self.client.set("hpsHeaders", headers)
        if json != None:
            data = jsonmod.dumps(json)
            self.client.set("hpsBody", data.encode())
        commandCode = self.METHOD_TO_CODE[method]
        self.client.set("hpsControlPoint", commandCode)

        fullStatus = self.client.get("hpsStatus")
        
        if VERBOSE:
            print(f"HPS status {repr(fullStatus)}")
        httpStatus, dataStatus = struct.unpack("<hb", fullStatus)
        if VERBOSE:
            print(f"HPS HTTP Status={httpStatus}, dataStatus=0x{dataStatus:x}")
        if httpStatus != 200:
            raise HpsError(f"HPS status code {httpStatus}")
        if dataStatus != 0 and dataStatus != 0x04:
            raise HpsError(f"HPS data status=0x{dataStatus:x}")
        rvBytes = self.client.get("hpsBody")
        if rvBytes == None or len(rvBytes) == 0:
            if VERBOSE:
                print(f"HPS {method} returned empty response")
            return None
        if VERBOSE:
            print(f"HPS {method} returned {rvBytes}")
        return jsonmod.loads(rvBytes)

    def get(self, endpoint, json=None):
        return self.request("GET", endpoint, json=json)

    def put(self, endpoint, json=None):
        return self.request("PUT", endpoint, json=json)

    def post(self, endpoint, json=None, files=None):
        assert files is None or json is None
        return self.request("POST", endpoint, json=json, files=files)
