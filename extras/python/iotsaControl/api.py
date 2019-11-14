from __future__ import print_function
from __future__ import absolute_import
from __future__ import unicode_literals
from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
import sys
import os
import subprocess
import copy
import time
import binascii
import io
import urllib.request
import requests.exceptions

from .consts import UserIntervention, IotsaError, CoapError, VERBOSE
from .dfu import DFU
from .wifi import IotsaWifi
from .protocols import HandlerForProto

class IotsaConfig(object):
    def __init__(self, device, api):
        self.device = device
        self.endpoint = api
        self.status = {}
        self.settings = {}
        self.didLoad = False

    def load(self):
        if self.didLoad: return
        self.status = self.device.protocolHandler.get(self.endpoint)
        self.didLoad = True
        
    def save(self):
        if self.settings:
            self.device.flush()
            reply = self.device.protocolHandler.put(self.endpoint, json=self.settings)
            if reply and reply.get('needsReboot'):
                msg = 'Reboot %s within %s seconds to activate mode %s' % (self.device.ipAddress, reply.get('requestedModeTimeout', '???'), self.device.modeName(reply.get('requestedMode')))
                raise UserIntervention(msg)
            
    def get(self, name, default=None):
        self.load()
        return self.status.get(name, default)
        
    def getAll(self):
        self.load()
        return copy.deepcopy(self.status)
        
    def set(self, name, value):
        self.settings[name] = value
        
    def printStatus(self):
        self.load()
        print('%s:' % self.device.ipAddress)
        for k, v in list(self.status.items()):
            print('  %-16s %s' % (str(k)+':', v))
            
class IotsaDevice(object):
    def __init__(self, ipAddress, port=None, protocol=None, noverify=False, bearer=None, auth=None):
        self.ipAddress = ipAddress
        self.protocolHandler = None
        if protocol == None: 
            protocol, noverify = self._guessProtocol(ipAddress, port)
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        self.protocolHandler = HandlerClass(url, noverify=noverify, bearer=bearer, auth=auth)
        self.config = IotsaConfig(self, 'config')
        self.auth = None
        self.bearerToken = None
        self.apis = {'config' : self.config}
        
    def __del__(self):
        self.close()
        
    def _guessProtocol(self, ipAddress, port):
        protocol = 'https'
        noverify = False
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.get('config')
        except requests.exceptions.SSLError:
            if VERBOSE: print("Using https protocol with --noverify")
            return 'https', True
        except:
            pass
        else:
            if VERBOSE: print("Using https protocol")
            return 'https', False
            
        protocol = 'http'
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.get('config')
        except:
            pass
        else:
            if VERBOSE: print("Using http protocol")
            return 'http', False
            
        protocol = 'coap'
        url = '%s://%s' % (protocol, ipAddress)
        if port:
            url += ':%d' % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.get('config')
        except:
            pass
        else:
            if VERBOSE: print("Using coap protocol")
            return 'coap', False
            
        raise IotsaError("Cannot determine protocol to use for {}, use --protocol".format(ipAddress))
            
            
    def close(self):
        if self.protocolHandler:
            self.protocolHandler.close()
        self.protocolHandler = None
        self.apis = None
        
    def flush(self):
        for a in self.apis.values():
            a.didLoad = False
        
    def setLogin(self, username, password):
        self.auth = (username, password)
        
    def setBearerToken(self, token):
        self.bearerToken = token
        
    def getApi(self, api):
        if not api in self.apis:
            self.apis[api] = IotsaConfig(self, api)
        return self.apis[api]
        
    def getAll(self):
        all = self.config.getAll()
        moduleNames = all.get('modules', [])
        modules = {}
        for m in moduleNames:
            if m == 'config':
                continue
            api = self.getApi(m)
            modules[m] = api.getAll()
        all['modules'] = modules
        return all
        
    def printStatus(self):
        status = self.config.getAll()
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
        
    def modeForName(self, name):
        names = ['normal', 'config', 'ota', 'factoryReset']
        return names.index(name)
        
    def gotoMode(self, modeName, wait=False, verbose=False):
        mode = self.modeForName(modeName)
        if self.config.get('currentMode') == mode:
            if verbose: print("%s: target already in mode %s" % (sys.argv[0], self.modeName(mode)))
            return
        self.config.set('requestedMode', mode)
        try:
            self.config.save()
        except UserIntervention as arg:
            if verbose:
                print("%s: %s" % (sys.argv[0], arg), file=sys.stderr)
            else:
                raise
        if not wait:
            return
        while True:
            time.sleep(5)
            self.flush()
            if self.config.get('currentMode') == mode:
                break
            reqMode = self.config.get('requestedMode', 0)
            if self.config.get('requestedMode') != mode:
                raise IotsaError("target now has requestedMode %s in stead of %s?" % (self.modeName(reqMode), self.modeName(mode)))
            if verbose:
                print("%s: Reboot %s within %s seconds to activate mode %s" % (sys.argv[0], self.ipAddress, self.config.get('requestedModeTimeout', '???'), self.modeName(reqMode)), file=sys.stderr)
        if verbose:
            print("%s: target is now in %s mode" % (sys.argv[0], self.modeName(mode)))

    def reboot(self):
        self.protocolHandler.put('config', json={'reboot':True})
        
    def ota(self, filename):
        if not os.path.exists(filename):
            filename, _ = urllib.request.urlretrieve(filename)
        ESPOTA="~/.platformio/packages/tool-espotapy/espota.py"
        ESPOTA = os.environ.get("ESPOTA", ESPOTA)
        ESPOTA = os.path.expanduser(ESPOTA)
        if not os.path.exists(ESPOTA):
            raise IotsaError("Helper command not found: %s\nPlease install espota.py and optionally set ESPOTA environment variable" % (ESPOTA))
        #cmd = [ESPOTA, '-i', self.ipAddress, '-f', filename]
        cmd = '"%s" -i %s -f "%s"' % (ESPOTA, self.ipAddress, filename)
        status = subprocess.call(cmd, shell=True)
        if status != 0:
            raise IotsaError("OTA command %s failed" % (cmd))

    def uploadCertificate(self, keyData, certificateData):
        if isinstance(keyData, str) and keyData.startswith('---'):
            keyData = keyData.splitlines()
            keyData = keyData[1:-1]
            keyData = '\n'.join(keyData)
            keyData = binascii.a2b_base64(keyData)
        if isinstance(certificateData, str) and certificateData.startswith('---'):
            certificateData = certificateData.splitlines()
            certificateData = certificateData[1:-1]
            certificateData = '\n'.join(certificateData)
            certificateData = binascii.a2b_base64(certificateData)
        keyFile = io.BytesIO(keyData)
        files = {
            'keyFile' : ('httpsKey.der', keyFile, 'application/binary'),
            }
        self.protocolHandler.post('/configupload', files=files)
        certificateFile = io.BytesIO(certificateData)
        files = {
            'certfile' : ('httpsCert.der', certificateFile, 'application/binary')
            }
        self.protocolHandler.post('/configupload', files=files)
        
