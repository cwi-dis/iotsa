import sys
import socket
import requests

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
        
class IotsaConfig:
    def __init__(self, device, api):
        self.device = device
        self.configURL = '%s://%s/api/%s' % (self.device.protocol, self.device.ipAddress, api)
        self.status = {}
        self.settings = {}

    def load(self):
        headers = {}
        if self.device.bearerToken:
            self.headers['Authorization'] = 'Bearer '+self.device.bearerToken
        if VERBOSE: print 'GET %s' % (self.configURL)
        r = requests.get(self.configURL, auth=self.device.auth, headers=headers)
        if VERBOSE: print 'returned: %s' % r.text
        r.raise_for_status()
        self.status = r.json()
        
    def save(self):
        if self.settings:
            headers = {}
            if self.device.bearerToken:
                self.headers['Authorization'] = 'Bearer '+self.device.bearerToken
            if VERBOSE: print 'PUT %s: %s' % (self.configURL, self.settings)
            r = requests.put(self.configURL, auth=self.device.auth, headers=headers, json=self.settings)
            self.settings = {}
            if VERBOSE: print 'returned: %s', r.text
            r.raise_for_status()
            self._handlePutReply(r)
            
    def _handlePutReply(self, r):
        if r.text and r.text[0] == '{':
            reply = r.json()
            if reply.get('needsReboot'):
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
    def __init__(self, ipAddress, protocol=None):
        self.ipAddress = ipAddress
        if protocol == None: 
            protocol = 'http'
        self.protocol = protocol
        IotsaConfig.__init__(self, self, 'config')
        self.auth = None
        self.bearerToken = None
        self.apis = {}
        
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
        print '%s:' % self.device.ipAddress
        print '  program:        ', self.status.get('program', 'unknown')
        print '  last boot:      ', 
        lastboot = self.status.get('uptime')
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
        lastreason = self.status.get('bootCause')
        if lastreason:
            print '(%s)' % lastreason,
        print
        print '  runmode:        ', self.modeName(self.status.get('currentMode', 0)),
        timeout = self.status.get('currentModeTimeout')
        if timeout:
            print '(%d seconds more)' % timeout,
        print
        if self.status.get('privateWifi'):
            print '     NOTE:         on private WiFi network'
        reqMode = self.status.get('requestedMode')
        if reqMode:
            print '  requested mode: ', self.modeName(reqMode),
            timeout = self.status.get('requestedModeTimeout', '???')
            if timeout:
                print '(needs reboot within %s seconds)' % timeout,
            print
        print '  hostname:       ', self.status.get('hostName', '')
        print '  iotsa:          ', self.status.get('iotsaFullVersion', '???')
        print '  modules:        ',
        for m in self.status.get('modules', ['???']):
            print m,
        print
            
    def modeName(self, mode):
        if mode is None: return 'unknown'
        mode = int(mode)
        names = ['normal', 'config', 'ota', 'factoryReset']
        if mode >= 0 and mode < len(names):
            return names[mode]
        return 'unknown-mode-%d' % mode
