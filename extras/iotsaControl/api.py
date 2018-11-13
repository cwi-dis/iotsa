from __future__ import print_function
from __future__ import absolute_import
from __future__ import unicode_literals
from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
import sys
import socket
import requests
import copy
import urllib.parse
from json import loads as json_loads
from json import dumps as json_dumps

VERBOSE=False
WITH_COAPTHON=True
if WITH_COAPTHON:
    # Coapthon is disabled for now: the current version automatically creates log
    # files and messes with the logger settings.
    import coapthon.client.helperclient
    import coapthon.defines

class UserIntervention(Exception):
    pass

class CoapError(Exception):
    pass

class PlatformWifi(object):
    """Default WiFi handling: asl the user."""
    def __init__(self):
        pass

    def platformListWifiNetworks(self):
        raise UserIntervention("Please look for WiFi SSIDs (network names) starting with 'config-'")

    def platformJoinWifiNetwork(self, ssid, password):
        raise UserIntervention("Please join WiFi network named %s" % ssid)
        
    def platformCurrentWifiNetworks(self):
        return []

class PlatformMDNSCollector(object):
    """Default mDNS handling: ask the user"""
    def __init__(self):
        pass
        
    def run(self, timeout=5):
        raise UserIntervention("Please browse for mDNS services _iotsa._tcp.local")
        
# Override the default WiFi and mDNS handling from os-dependent implementations
from .machdep import *
from . import machdep
        
class IotsaWifi(PlatformWifi):
    def __init__(self):
        PlatformWifi.__init__(self)
        self.ssid = None
        self.device = None
        
    def findNetworks(self):
        all = self.platformListWifiNetworks()
        rv = []
        for net in all:
            if net.startswith('config-'):
                rv.append(net)
        return rv
        
    def _isNetworkSelected(self, ssid):
        return ssid in self.platformCurrentWifiNetworks()

    def _isConfigNetwork(self):
        if self.ssid and self.ssid.startswith('config-'):
            return True
        for c in self.platformCurrentWifiNetworks():
            if c.startswith('config-'):
                return True
        return False
                
    def selectNetwork(self, ssid, password=None):
        if self._isNetworkSelected(ssid):
            return True
        ok = self.platformJoinWifiNetwork(ssid, password)
        if ok:
            self.ssid = ssid
        return ok
        
    def _checkDevice(self, deviceName):
        ports = [80, 443]
        for port in ports:
            try:
                _ = socket.create_connection((deviceName, port), 20)
            except socket.timeout:
                return False
            except socket.gaierror:
                print('Unknown host: %s' % deviceName, file=sys.stderr)
                return False
            except socket.error:
                continue
            return True
        return False
        
    def findDevices(self):
        if self._isConfigNetwork():
            if self._checkDevice('192.168.4.1'):
                return ['192.168.4.1']
        collect = PlatformMDNSCollector()
        devices = collect.run()
        return devices
        #raise UserIntervention("Please use Bonjour or avahi to find WiFi devices serving _iotsa._tcp")
        
    def selectDevice(self, device):
        if self._checkDevice(device):
            self.device = device
            return True
        return False
        
    def currentDevice(self):
        return self.device
        
class IotsaRESTProtocolHandler(object):
    def __init__(self, baseURL, noverify=False):
        if baseURL[-1] != '/':
            baseURL += '/'
        baseURL += 'api/'
        self.baseURL = baseURL
        self.noverify = noverify
        
    def close(self):
        pass

    def get(self, endpoint, auth=None, token=None, json=None):
        return self.request('GET', endpoint, auth=auth, token=token, json=json)
        
    def put(self, endpoint, auth=None, token=None, json=None):
        return self.request('PUT', endpoint, auth=auth, token=token, json=json)
        
    def post(self, endpoint, auth=None, token=None, json=None):
        return self.request('POST', endpoint, auth=auth, token=token, json=json)
        
    def request(self, method, endpoint, auth=None, token=None, json=None):
        headers = {}
        if token:
            headers['Authorization'] = 'Bearer '+token
        url = self.baseURL + endpoint
        if VERBOSE: print('REST %s %s' % (method, url))
        r = requests.request(method, url, auth=auth, json=json, verify=not self.noverify)
        if VERBOSE: print('REST %s returned: %s %s' % (method, r.status_code, r.text))
        r.raise_for_status()
        if r.text and r.text[0] == '{':
            return r.json()
        return None

class IotsaCOAPProtocolHandler(object):
    def __init__(self, baseURL):
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
            
    def get(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is None
        endpoint = self.basePath+endpoint
        if VERBOSE: print('COAP GET coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint))
        rv = self.client.get(endpoint)
        if VERBOSE: print('COAP GET returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json_loads(rv.payload)
        
    def put(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is not None
        endpoint = self.basePath+endpoint
        data = json_dumps(json)
        if VERBOSE: print('COAP PUT coap://%s:%d%s %s' % (self.client.server[0], self.client.server[1], endpoint, data))
        rv = self.client.put(endpoint, (coapthon.defines.Content_types['application/json'], data))
        if VERBOSE: print('COAP PUT returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json_loads(rv.payload)
        
    def post(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is not None
        endpoint = self.basePath+endpoint
        data = json_dumps(json)
        if VERBOSE: print('COAP POST coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint, data))
        rv = self.client.post(endpoint, (coapthon.defines.Content_types['application/json'], data))
        if VERBOSE: print('COAP POST returned', rv.code, repr(rv.payload))
        self._raiseIfError(rv)
        return json_loads(rv.payload)
        
HandlerForProto = {
    'http' : IotsaRESTProtocolHandler,
    'https' : IotsaRESTProtocolHandler,
    'coap' : IotsaCOAPProtocolHandler,
}

class IotsaConfig(object):
    def __init__(self, device, api):
        self.device = device
        self.endpoint = api
        self.status = {}
        self.settings = {}

    def load(self):
        self.status = self.device.protocolHandler.get(self.endpoint, auth=self.device.auth, token=self.device.bearerToken)
        
    def save(self):
        if self.settings:
            reply = self.device.protocolHandler.put(self.endpoint, auth=self.device.auth, token=self.device.bearerToken, json=self.settings)
            if reply and reply.get('needsReboot'):
                msg = 'Reboot %s within %s seconds to activate mode %s' % (self.device.ipAddress, reply.get('requestedModeTimeout', '???'), self.device.modeName(reply.get('requestedMode')))
                raise UserIntervention(msg)
            
    def get(self, name, default=None):
        return self.status.get(name, default)
        
    def set(self, name, value):
        self.settings[name] = value
        
    def printStatus(self):
        print('%s:' % self.device.ipAddress)
        for k, v in list(self.status.items()):
            print('  %-16s %s' % (str(k)+':', v))
            
class IotsaDevice(IotsaConfig):
    def __init__(self, ipAddress, port=None, protocol=None, noverify=False):
        self.ipAddress = ipAddress
        if protocol == None: 
            protocol = 'http'
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        self.protocolHandler = HandlerClass(url, noverify=noverify)
        IotsaConfig.__init__(self, self, 'config')
        self.auth = None
        self.bearerToken = None
        self.apis = {}
        
    def __del__(self):
        self.close()
        
    def close(self):
        if self.protocolHandler:
            self.protocolHandler.close()
        self.protocolHandler = None
        self.apis = None
        
    def setLogin(self, username, password):
        self.auth = (username, password)
        
    def setBearerToken(self, token):
        self.bearerToken = token
        
    def getApi(self, api):
        if api == 'config': return self
        if not api in self.apis:
            self.apis[api] = IotsaConfig(self, api)
        return self.apis[api]
        
    def printStatus(self):
        status = copy.deepcopy(self.status)
        print('%s:' % self.ipAddress)
        print('  program:        ', status.pop('program', 'unknown'))
        print('  last boot:      ', end=' ') 
        lastboot = status.pop('uptime')
        if not lastboot:
            print('???', end=' ')
        else:
            if lastboot <= 60:
                print('%ds' % lastboot, end=' ')
            else:
                lastboot /= 60
                if lastboot <= 60:
                    print('%dm' % lastboot, end=' ')
                else:
                    lastboot /= 60
                    if lastboot <= 24:
                        print('%dh' % lastboot, end=' ')
                    else:
                        lastboot /= 24
                        print('%dd' % lastboot, end=' ')
        lastreason = status.pop('bootCause')
        if lastreason:
            print('(%s)' % lastreason, end=' ')
        print()
        print('  runmode:        ', self.modeName(status.pop('currentMode', 0)), end=' ')
        timeout = status.pop('currentModeTimeout', None)
        if timeout:
            print('(%d seconds more)' % timeout, end=' ')
        print()
        if status.pop('privateWifi', False):
            print('     NOTE:         on private WiFi network')
        reqMode = status.pop('requestedMode', None)
        if reqMode:
            print('  requested mode: ', self.modeName(reqMode), end=' ')
            timeout = status.pop('requestedModeTimeout', '???')
            if timeout:
                print('(needs reboot within %s seconds)' % timeout, end=' ')
            print()
        print('  hostName:       ', status.pop('hostName', ''))
        iotsaVersion = status.pop('iotsaVersion', '???')
        print('  iotsa:          ', status.pop('iotsaFullVersion', iotsaVersion))
        print('  modules:        ', end=' ')
        for m in status.pop('modules', ['???']):
            print(m, end=' ')
        print()
        for k, v in list(status.items()):
            print('  %-16s %s' % (k+':', v))
            
    def modeName(self, mode):
        if mode is None: return 'unknown'
        mode = int(mode)
        names = ['normal', 'config', 'ota', 'factoryReset']
        if mode >= 0 and mode < len(names):
            return names[mode]
        return 'unknown-mode-%d' % mode
