from __future__ import print_function
from __future__ import absolute_import
from __future__ import unicode_literals
from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
import sys
import socket
import time
import urllib.parse
import urllib.request
import json
import coapthon.client.helperclient
import coapthon.defines
import requests
# We disable warnings about invalid certificates: we expect them and they provide no meaningful information
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

from .consts import UserIntervention, IotsaError, CoapError, VERBOSE
        
class IotsaRESTProtocolHandler(object):
    def __init__(self, baseURL, noverify=False, bearer=None, auth=None):
        if baseURL[-1] != '/':
            baseURL += '/'
        baseURL += 'api/'
        self.baseURL = baseURL
        self.noverify = noverify
        self.bearer = bearer
        self.auth = auth
        
    def close(self):
        pass

    def get(self, endpoint, json=None):
        return self.request('GET', endpoint, json=json)
        
    def put(self, endpoint, json=None):
        return self.request('PUT', endpoint, json=json)
        
    def post(self, endpoint, json=None, files=None):
        assert files is None or json is None
        return self.request('POST', endpoint, json=json, files=files)
        
    def request(self, method, endpoint, json=None, files=None):
        headers = {}
        if self.bearer:
            headers['Authorization'] = 'Bearer '+self.bearer
        url = self.baseURL + endpoint
        if VERBOSE: 
            print('REST %s %s' % (method, url))
            if self.auth:
                print('auth', self.auth)
            if headers:
                print('....', headers)
            if json:
                print('>>>>', json)
        retryCount = 5
        while True:
            try:
                r = requests.request(method, url, auth=self.auth, json=json, verify=not self.noverify, headers=headers, files=files)
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
            print('Note: received redirect when accessing', url)
        if VERBOSE: print('<<<< status=%s reply=%s' % (r.status_code, r.text))
        r.raise_for_status()
        if r.text and r.text[0] == '{':
            return r.json()
        return None

class IotsaCOAPProtocolHandler(object):
    def __init__(self, baseURL, bearer=None, noverify=None, auth=None):
        assert bearer is None
        assert noverify is None
        assert auth is None
        parts = urllib.parse.urlparse(baseURL)
        self.basePath = parts.path
        if not self.basePath:
            self.basePath = '/'
        assert not parts.params
        assert not parts.query
        assert not parts.fragment
        assert parts.netloc
        if ':' in parts.netloc:
            host, port = parts.netloc.split(':')
            port = int(port)
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
        if reply.code >= 128:
            codeMajor = (reply.code >> 5) & 0x07
            codeMinor = (reply.code & 0x1f)
            codeName = coapthon.defines.Codes.LIST.get(reply.code, None)
            if codeName:
                codeName = codeName.name
            else:
                codeName = 'unknown'
            raise CoapError('%d.%02d %s' % (codeMajor, codeMinor, codeName))
            
    def get(self, endpoint, json=None):
        assert json is None
        endpoint = self.basePath+endpoint
        if VERBOSE: print('COAP GET coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint))
        rv = self.client.get(endpoint)
        if VERBOSE: print('COAP GET returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json.loads(rv.payload)
        
    def put(self, endpoint, json=None):
        assert json is not None
        endpoint = self.basePath+endpoint
        data = json.dumps(json)
        if VERBOSE: print('COAP PUT coap://%s:%d%s %s' % (self.client.server[0], self.client.server[1], endpoint, data))
        rv = self.client.put(endpoint, (coapthon.defines.Content_types['application/json'], data))
        if VERBOSE: print('COAP PUT returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json.loads(rv.payload)
        
    def post(self, endpoint, json=None, files=None):
        assert json is not None
        assert files is None
        endpoint = self.basePath+endpoint
        data = json.dumps(json)
        if VERBOSE: print('COAP POST coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint, data))
        rv = self.client.post(endpoint, (coapthon.defines.Content_types['application/json'], data))
        if VERBOSE: print('COAP POST returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json.loads(rv.payload)
        
HandlerForProto = {
    'http' : IotsaRESTProtocolHandler,
    'https' : IotsaRESTProtocolHandler,
    'coap' : IotsaCOAPProtocolHandler,
}
