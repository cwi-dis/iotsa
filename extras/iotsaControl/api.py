import sys
import socket
import requests
import copy
import coapthon.client.helperclient
import urlparse
from json import loads as json_loads
from json import dumps as json_dumps

VERBOSE=False

class UserIntervention(Exception):
    pass

class PlatformWifi:
    """Default WiFi handling: asl the user."""
    def __init__(self):
        pass

    def platformListWifiNetworks(self):
        raise UserIntervention("Please look for WiFi SSIDs (network names) starting with 'config-'")

    def platformJoinWifiNetwork(self, ssid, password):
        raise UserIntervention("Please join WiFi network named %s" % ssid)
        
    def platformCurrentWifiNetworks(self):
        return []

class PlatformMDNSCollector:
    """Default mDNS handling: ask the user"""
    def __init__(self):
        pass
        
    def run(self, timeout=5):
        raise UserIntervention("Please browse for mDNS services _iotsa._tcp.local")
        
# Override the default WiFi and mDNS handling from os-dependent implementations
from machdep import *
import machdep
        
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
        try:
            _ = socket.create_connection((deviceName, 80), 20)
        except socket.timeout:
            return False
        except socket.gaierror:
            print >>sys.stderr, 'Unknown host: %s' % deviceName
            return False
        return True
        
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
        
class IotsaRESTProtocolHandler:
    def __init__(self, baseURL):
        if baseURL[-1] != '/':
            baseURL += '/'
        baseURL += 'api/'
        self.baseURL = baseURL
        
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
        if VERBOSE: print 'REST %s %s' % (method, url)
        r = requests.request(method, url, auth=auth, json=json)
        if VERBOSE: print 'REST %s returned: %s' % (method, r.text)
        r.raise_for_status()
        if r.text and r.text[0] == '{':
            return r.json()
        return None

class IotsaCOAPProtocolHandler:
    def __init__(self, baseURL):
        parts = urlparse.urlparse(baseURL)
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
        
    def get(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is None
        endpoint = self.basePath+endpoint
        if VERBOSE: print 'COAP GET coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint)
        rv = self.client.get(endpoint)
        if VERBOSE: print 'COAP GET returned', repr(rv.payload)
        return json_loads(rv.payload)
        
    def put(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is not None
        endpoint = self.basePath+endpoint
        if VERBOSE: print 'COAP PUT coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint)
        rv = self.client.put(endpoint, json_dumps(json))
        if VERBOSE: print 'COAP PUT returned', rv.code, repr(rv.payload)
        return json_loads(rv.payload)
        
    def post(self, endpoint, auth=None, token=None, json=None):
        assert auth is None
        assert token is None
        assert json is not None
        endpoint = self.basePath+endpoint
        if VERBOSE: print 'COAP POST coap://%s:%d%s' % (self.client.server[0], self.client.server[1], endpoint)
        rv = self.client.post(endpoint, json_dumps(json))
        if VERBOSE: print 'COAP POST returned', repr(rv.payload)
        return json_loads(rv.payload)
        
HandlerForProto = {
    'http' : IotsaRESTProtocolHandler,
    'https' : IotsaRESTProtocolHandler,
    'coap' : IotsaCOAPProtocolHandler,
}

class IotsaConfig:
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
                msg = 'Reboot %s within %s seconds to activate mode %s' % (self.ipAddress, reply.get('requestedModeTimeout', '???'), self.modeName(reply.get('requestedMode')))
                raise UserIntervention(msg)
            
    def get(self, name, default=None):
        return self.status.get(name, default)
        
    def set(self, name, value):
        self.settings[name] = value
        
    def printStatus(self):
        print '%s:' % self.device.ipAddress
        for k, v in self.status.items():
            print '  %-16s %s' % (str(k)+':', v)
            
class IotsaDevice(IotsaConfig):
    def __init__(self, ipAddress, port=None, protocol=None):
        self.ipAddress = ipAddress
        if protocol == None: 
            protocol = 'http'
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        self.protocolHandler = HandlerClass(url)
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
        print '%s:' % self.device.ipAddress
        print '  program:        ', status.pop('program', 'unknown')
        print '  last boot:      ', 
        lastboot = status.pop('uptime')
        if not lastboot:
            print '???',
        else:
            if lastboot <= 60:
                print '%ds' % lastboot,
            else:
                lastboot /= 60
                if lastboot <= 60:
                    print '%dm' % lastboot,
                else:
                    lastboot /= 60
                    if lastboot <= 24:
                        print '%dh' % lastboot,
                    else:
                        lastboot /= 24
                        print '%dd' % lastboot,
        lastreason = status.pop('bootCause')
        if lastreason:
            print '(%s)' % lastreason,
        print
        print '  runmode:        ', self.modeName(status.pop('currentMode', 0)),
        timeout = status.pop('currentModeTimeout', None)
        if timeout:
            print '(%d seconds more)' % timeout,
        print
        if status.pop('privateWifi', False):
            print '     NOTE:         on private WiFi network'
        reqMode = status.pop('requestedMode', None)
        if reqMode:
            print '  requested mode: ', self.modeName(reqMode),
            timeout = status.pop('requestedModeTimeout', '???')
            if timeout:
                print '(needs reboot within %s seconds)' % timeout,
            print
        print '  hostName:       ', status.pop('hostName', '')
        iotsaVersion = status.pop('iotsaVersion', '???')
        print '  iotsa:          ', status.pop('iotsaFullVersion', iotsaVersion)
        print '  modules:        ',
        for m in status.pop('modules', ['???']):
            print m,
        print
        for k, v in status.items():
            print '  %-16s %s' % (k+':', v)
            
    def modeName(self, mode):
        if mode is None: return 'unknown'
        mode = int(mode)
        names = ['normal', 'config', 'ota', 'factoryReset']
        if mode >= 0 and mode < len(names):
            return names[mode]
        return 'unknown-mode-%d' % mode
