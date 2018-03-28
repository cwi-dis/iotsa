#!/usr/bin/env python
import argparse
import os
import sys
import socket
import urlparse

VERBOSE=False

class UserIntervention(Exception):
    pass
    
class IotsaWifi:
    def __init__(self):
        self.ssid = None
        self.device = None
        
    def findNetworks(self):
        raise UserIntervention("Please look for WiFi SSIDs (network names) starting with 'config-'")
        
    def _isNetworkSelected(self, ssid):
        return False

    def _isConfigNetwork(self):
        return self.ssid and self.ssid.startswith('config-')
                
    def selectNetwork(self, ssid, password=None):
        if self._isNetworkSelected(ssid):
            return True
        raise UserIntervention("Please join WiFi network named %s" % ssid)
        
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
        raise UserIntervention("Please use Bonjour or avahi to find WiFi devices serving _iotsa._tcp")
        
    def selectDevice(self, device):
        if self._checkDevice(device):
            self.device = device
            return True
        return False
        
    def currentDevice(self):
        return self.device
        
class IotsaConfig:
    def __init__(self, device):
        self.device = device
        self.configURL = urlparse.urlunparse('http', self.device, '/api/config')
        self.wifiConfigURL = urlparse.urlunparse('http', self.device, '/api/wificonfig')
        self.status = {}
        self.configSettings = {}
        self.wifiConfigSettings = {}
        self.auth = None
        self.bearer_token = None
        
    def setLogin(self, username, password):
        self.auth = (username, password)
        
    def setBearerToken(self, token):
        self.bearerToken = token
        
    def load(self):
        headers = {}
        if self.bearer_token:
            self.headers['Authorization'] = 'Bearer '+self.bearerToken
        r = requests.get(self.configURL, auth=self.auth, headers=headers)
        r.raise_for_status()
        self.status = r.json()
        
    def save(self):
        if self.configSettings:
            headers = {}
            if self.bearer_token:
                self.headers['Authorization'] = 'Bearer '+self.bearerToken
            r = requests.put(self.configURL, auth=self.auth, headers=headers, json=self.configSettings)
            r.raise_for_status()
        if self.wifiSettings:
            headers = {}
            if self.bearer_token:
                self.headers['Authorization'] = 'Bearer '+self.bearerToken
            r = requests.put(self.wifiConfigURL, auth=self.auth, headers=headers, json=self.wifiSettings)
            r.raise_for_status()
            
class Main:
    def __init__(self):
        self.wifi = None
        self.device = None
        
    def run(self):
        self.parseArgs()
        for cmd in self.args.command:
            cmdName = 'cmd_' + cmd
            if not hasattr(self, cmdName):
                print >>sys.stderr, "%s: unknown command %s, help for help" % (sys.argv[0], cmd)
                sys.exit(1)
            handler = getattr(self, cmdName)
            try:
                handler()
            except UserIntervention, arg:
                print >>sys.stderr, "%s: user intervention required:" % sys.argv[0]
                print >>sys.stderr, "%s: %s" % (sys.argv[0], arg)
                sys.exit(2)
            
    def cmd_help(self):
        """List available commands"""
        for name in dir(self):
            if not name.startswith('cmd_'): continue
            handler = getattr(self, name)
            print '%-10s\t%s' % (name[4:], handler.__doc__)
            
    def cmd_networks(self):
        """List iotsa wifi networks"""
        self.loadWifi()
        networks = self.wifi.findNetworks()
        for n in networks: print n
        
    def cmd_targets(self):
        """List iotsa devices visible on current network"""
        self.loadWifi()
        targets = self.wifi.findDevices()
        for t in targets: print t

    def parseArgs(self):
        global VERBOSE
        parser = argparse.ArgumentParser(description="Access Igor home automation service and other http databases")
        parser.add_argument("--ssid", action="store", metavar="SSID", help="Connect to WiFi network named SSID")
        parser.add_argument("--ssidpw", action="store", metavar="password", help="WiFi password for network SSID")
        parser.add_argument("--target", "-t", action="store", metavar="IP", help="Iotsa board to operate on (use \"auto\" for automatic)")
    
    #    parser.add_argument("-u", "--url", help="Base URL of the server (default: %s, environment IGORSERVER_URL)" % CONFIG.get('igor', 'url'))
        parser.add_argument("--verbose", action="store_true", help="Print what is happening")
        parser.add_argument("--bearer", metavar="TOKEN", help="Add Authorization: Bearer TOKEN header line")
        parser.add_argument("--access", metavar="TOKEN", help="Add access_token=TOKEN query argument")
        parser.add_argument("--credentials", metavar="USER:PASS", help="Add Authorization: Basic header line with given credentials")
    #    parser.add_argument("--noverify", action='store_true', help="Disable verification of https signatures")
    #    parser.add_argument("--certificate", metavar='CERTFILE', help="Verify https certificates from given file")
        parser.add_argument("command", nargs="+", help="Command to run (help for list)")
        self.args = parser.parse_args()
        VERBOSE=self.args.verbose

    def loadWifi(self):
        if self.wifi: return
        self.wifi = IotsaWifi()
        if self.args.ssid:
            ok = self.wifi.selectNetwork(self.args.ssid, self.args.ssidpw)
            if not ok:
                print >>sys.stderr, "%s: cannot select wifi network %s" % (sys.argv[0], self.args.ssid)
                sys.exit(1)
        if self.args.target == "auto":
            all = self.wifi.findDevices()
            if len(all) == 0:
                print >>sys.stderr, "%s: no iotsa devicves found" % (sys.argv[0])
                sys.exit(1)
            if len(all) > 1:
                print >>sys.stderr, "%s: multiple iotsa devices:" % (sys.argv[0]),
                for a in all:
                    print a,
                print
                sys.exit(1)
            self.args.target = all[0]
        if self.args.target:
            ok = self.wifi.selectDevice(self.args.target)
        return

    def loadDevice(self):
        self.loadWifi()
        
def main():
    m = Main()
    m.run()
    
if __name__ == '__main__':
    main()
    
