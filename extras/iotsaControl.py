#!/usr/bin/env python
import argparse
import os
import sys
import socket
import urlparse
import requests
import time

VERBOSE=False

class UserIntervention(Exception):
    pass

if sys.platform == 'darwin':
    import subprocess
    import plistlib
    class PlatformWifi:
        def __init__(self):
            self.wifiInterface = 'en2'

        def platformListWifiNetworks(self):
            if VERBOSE: print 'Listing wifi networks (OSX)'
            p = subprocess.Popen('/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport --scan --xml', shell=True, stdout=subprocess.PIPE)
            data = plistlib.readPlist(p.stdout)
            wifiNames = map(lambda d : d['SSID_STR'], data)
            wifiNames.sort()
            if VERBOSE: print 'Wifi networks found:', wifiNames
            return wifiNames

        def platformJoinWifiNetwork(self, ssid, password):
            if VERBOSE: print 'Joining network (OSX):', ssid
            cmd = 'networksetup -setairportnetwork %s %s' % (self.wifiInterface, ssid)
            if password:
                cmd += ' ' + password
            status = p = subprocess.call(cmd, shell=True)
            if VERBOSE: print 'Join network status:', status
            return status == 0

        def platformCurrentWifiNetworks(self):
            if VERBOSE: print 'Find current networks (OSX)'
            cmd = 'networksetup -getairportnetwork %s' % self.wifiInterface
            p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
            data = p.stdout.read()
            if VERBOSE: print 'Find result was:', data
            wifiName = data.split(':')[-1]
            wifiName = wifiName.strip()
            return [wifiName]
else:
    class PlatformWifi:
        def __init__(self):
            pass

        def platformListWifiNetworks(self):
            raise UserIntervention("Please look for WiFi SSIDs (network names) starting with 'config-'")

        def platformJoinWifiNetwork(self, ssid, password):
            raise UserIntervention("Please join WiFi network named %s" % ssid)
            
        def platformCurrentWifiNetworks(self):
            return []
    
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
        raise UserIntervention("Please use Bonjour or avahi to find WiFi devices serving _iotsa._tcp")
        
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
        if VERBOSE: print 'returned: %s', r.text
        r.raise_for_status()
        self.status = r.json()
        
    def save(self):
        if self.settings:
            headers = {}
            if self.device.bearerToken:
                self.headers['Authorization'] = 'Bearer '+self.device.bearerToken
            if VERBOSE: print 'PUT %s: %s' % (self.configURL, self.settings)
            r = requests.put(self.configURL, auth=self.device.auth, headers=headers, json=self.settings)
            if VERBOSE: print 'returned: %s', r.text
            r.raise_for_status()
            self._handlePutReply(r)
            
    def _handlePutReply(self, r):
        if r.text and r.text[0] == '{':
            reply = r.json()
            if reply['needsReboot']:
                msg = 'Reboot %s within %s seconds to activate mode %s' % (self.ipAddress, reply.get('requestedModeTimeout', '???'), self.modeName(reply.get('requestedMode')))
                raise UserIntervention(msg)
            
    def get(self, name, default=None):
        return self.status.get(name, default)
        
    def set(self, name, value):
        self.settings[name] = value
        
        
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
        print '%s:' % self.ipAddress
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
        
class Main:
    def __init__(self):
        self.wifi = None
        self.device = None
        self.cmdlist = []
        
    def _getcmd(self):
        if not self.cmdlist: return None
        return self.cmdlist.pop(0)
        
    def _ungetcmd(self, cmd):
        self.cmdlist.insert(0, cmd)
        
    def run(self):
        self.parseArgs()
        self.cmdlist = self.args.command
        if type(self.cmdlist) != type([]):
            self.cmdlist = [self.cmdlist]
        while True:
            cmd = self._getcmd()
            if not cmd: break
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
        
    def cmd_info(self):
        """Show information on current device"""
        self.loadDevice()
        self.device.printStatus()
        
    def cmd_configMode(self):
        """Ask device to go into configuration mode"""
        self.loadDevice()
        self.device.set('requestedMode', 1)
        self.device.save()

    def cmd_configWait(self):
        """Ask device to go into configuration mode and wait until it is (probably after user intervention)"""
        self._configModeAndWait(1)
        
    def cmd_otaMode(self):
        """Ask device to go into over-the-air programming mode"""
        self.loadDevice()
        self.device.set('requestedMode', 2)
        self.device.save()

    def cmd_otaWait(self):
        """Ask device to go into over-the-air programming mode and wait until it is (probably after user intervention)"""
        self._configModeAndWait(2)
        
    def _configModeAndWait(self, mode):
        self.loadDevice()
        self.device.set('requestedMode', mode)
        try:
            self.device.save()
        except UserIntervention, arg:
            print >>sys.stderr, "%s: %s" % (sys.argv[0], arg)
        while True:
            time.sleep(5)
            self.device.load()
            if self.device.get('currentMode') == mode:
                break
            reqMode = self.device.get('requestedMode', 0)
            if self.device.get('requestedMode') != mode:
                print >>sys.stderr, "%s: target now has requestedMode %s in stead of %s?" % (sys.argv[0], self.device.modeName(reqMode), self.device.modeName(mode))
            print >>sys.stderr, "%s: Reboot %s within %s seconds to activate mode %s" % (sys.argv[0], self.device.ipAddress, self.device.get('requestedModeTimeout', '???'), self.device.modeName(reqMode))
            
    def cmd_factoryReset(self):
        """Ask device to do a factory-reset"""
        self.loadDevice()
        self.device.set('requestedMode', 3)
        self.device.save()

    def cmd_config(self):
        """Set configuration mode parameters"""
        self.loadDevice()
        if self.device.get('currentMode', 0) != 1:
            raise UserIntervention("Set target into configuration mode first. See configMode or configWait commands.")
        
        anyDone = False
        while True:
            subCmd = self._getcmd()
            if not subCmd or subCmd == '--':
                break
            if not '=' in subCmd:
                self._ungetcmd(subCmd)
                break
            name, rest = subCmd.split('=')
            if type(rest) == type(()):
                value = '='.join(rest)
            else:
                value = rest
            self.device.set(name, value)
            anyDone = True
        if not anyDone:
            print >>sys.stderr, "%s: config: requires name=value [...] to set config variables" % sys.argv[0]
            sys.exit(1)
        self.device.save()
        
    def cmd_wifi(self):
        self.loadDevice()
        if not self.device.get('privateWifi') and self.device.get('currentMode', 0) != 1:
            raise UserIntervention("Set target into configuration mode first. See configMode or configWait commands.")
        
        anyDone = False
        while True:
            subCmd = self._getcmd()
            if not subCmd or subCmd == '--':
                break
            if not '=' in subCmd:
                self._ungetcmd(subCmd)
                break
            name, rest = subCmd.split('=')
            if type(rest) == type(()):
                value = '='.join(rest)
            else:
                value = rest
            self.device.set(name, value)
            anyDone = True
        if not anyDone:
            print >>sys.stderr, "%s: config: requires name=value [...] to set config variables" % sys.argv[0]
            sys.exit(1)
        self.device.save()
        
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
        return

    def loadDevice(self):
        if self.device: return
        self.loadWifi()
        if not self.args.target or self.args.target == "auto":
            all = self.wifi.findDevices()
            if len(all) == 0:
                print >>sys.stderr, "%s: no iotsa devices found" % (sys.argv[0])
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
        self.device = IotsaDevice(self.wifi.currentDevice())
        if self.args.bearer:
            self.device.setBearerToken(self.args.bearer)
        if self.args.credentials:
            self.device.setCredentials(*self.args.credentials.split(':'))
        self.device.load()
        
def main():
    m = Main()
    m.run()
    
if __name__ == '__main__':
    main()
    
