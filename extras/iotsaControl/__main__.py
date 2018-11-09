#!/usr/bin/env python
from __future__ import print_function
from __future__ import absolute_import
from future import standard_library
standard_library.install_aliases()
from builtins import object
import argparse
import sys
import requests
import time
from . import api
import os
import subprocess
from . import machdep
import urllib.request, urllib.parse, urllib.error
import socket

orig_getaddrinfo = socket.getaddrinfo
def ipv4_getaddrinfo(host, port, family=0, socktype=0, proto=0, flags=0):
    if family == 0:
        family = socket.AF_INET
    return orig_getaddrinfo(host, port, family, socktype, proto, flags)
    
class Main(object):
    """Main commandline program"""
    
    def __init__(self):
        self.wifi = None
        self.device = None
        self.cmdlist = []
        
    def __del__(self):
        self.close()
        
    def close(self):
        self.wifi = None
        if self.device:
            self.device.close()
        self.device = None
        
    def run(self):
        """Run the main commandline program"""
        self.parseArgs()
        self.cmdlist = self.args.command
        if type(self.cmdlist) != type([]):
            self.cmdlist = [self.cmdlist]
        try:
            while True:
                cmd = self._getcmd()
                if not cmd: break
                cmdName = 'cmd_' + cmd
                if not hasattr(self, cmdName):
                    print("%s: unknown command %s, help for help" % (sys.argv[0], cmd), file=sys.stderr)
                    sys.exit(1)
                handler = getattr(self, cmdName)
                try:
                    handler()
                except api.UserIntervention as arg:
                    print("%s: %s: user intervention required:" % (sys.argv[0], cmd), file=sys.stderr)
                    print("%s: %s" % (sys.argv[0], arg), file=sys.stderr)
                    sys.exit(2)
                except requests.exceptions.HTTPError as arg:
                    print("%s: %s: %s" % (sys.argv[0], cmd, arg), file=sys.stderr)
                    sys.exit(1)
                except api.CoapError as arg:
                    print("%s: %s: %s" % (sys.argv[0], cmd, arg), file=sys.stderr)
                    sys.exit(1)
        finally:
            self.close()

    def _configModeAndWait(self, mode):
        """Helper method to request a specific mode and wait until the user has enabled it"""
        self.loadDevice()
        if self.device.get('currentMode') == mode:
            print("%s: target already in mode %s" % (sys.argv[0], self.device.modeName(mode)))
            return
        self.device.set('requestedMode', mode)
        try:
            self.device.save()
        except api.UserIntervention as arg:
            print("%s: %s" % (sys.argv[0], arg), file=sys.stderr)
        while True:
            time.sleep(5)
            self.device.load()
            if self.device.get('currentMode') == mode:
                break
            reqMode = self.device.get('requestedMode', 0)
            if self.device.get('requestedMode') != mode:
                print("%s: target now has requestedMode %s in stead of %s?" % (sys.argv[0], self.device.modeName(reqMode), self.device.modeName(mode)), file=sys.stderr)
            print("%s: Reboot %s within %s seconds to activate mode %s" % (sys.argv[0], self.device.ipAddress, self.device.get('requestedModeTimeout', '???'), self.device.modeName(reqMode)), file=sys.stderr)
        print("%s: target is now in %s mode" % (sys.argv[0], self.device.modeName(mode)))
        
    def parseArgs(self):
        """Command line argument handling"""
        parser = argparse.ArgumentParser(description="Access Igor home automation service and other http databases")
        parser.add_argument("--ssid", action="store", metavar="SSID", help="Connect to WiFi network named SSID")
        parser.add_argument("--ssidpw", action="store", metavar="password", help="WiFi password for network SSID")
        parser.add_argument("--target", "-t", action="store", metavar="IP", help="Iotsa board to operate on (use \"auto\" for automatic)")
        parser.add_argument("--protocol", action="store", metavar="PROTO", help="Access protocol (default: http, allowed: https, coap)")
        parser.add_argument("--port", action="store", metavar="PROTO", help="Port number (default depends on protocol)")
    
    #    parser.add_argument("-u", "--url", help="Base URL of the server (default: %s, environment IGORSERVER_URL)" % CONFIG.get('igor', 'url'))
        parser.add_argument("--ipv6", action="store_true", help="Allow IPv6. Default is to monkey-patch out IPv6 addresses to work around esp8266 mDNS bug.")
        parser.add_argument("--verbose", action="store_true", help="Print what is happening")
        parser.add_argument("--bearer", metavar="TOKEN", help="Add Authorization: Bearer TOKEN header line")
        parser.add_argument("--access", metavar="TOKEN", help="Add access_token=TOKEN query argument")
        parser.add_argument("--credentials", metavar="USER:PASS", help="Add Authorization: Basic header line with given credentials")
        parser.add_argument("--noverify", action='store_true', help="Disable verification of https signatures")
    #    parser.add_argument("--certificate", metavar='CERTFILE', help="Verify https certificates from given file")
        parser.add_argument("--compat", action="store_true", help="Compatability for old iotsa devices (ota only)")
        parser.add_argument("command", nargs="+", help="Command to run (help for list)")
        self.args = parser.parse_args()
        api.VERBOSE=self.args.verbose
        machdep.VERBOSE=self.args.verbose
        if not self.args.ipv6:
            # Current esp8266 Arduino mDNS implementation has a problem: it replies with an IPv4 address but does not send a
            # negative reply for the IPv6 address. This causes requests to retry the mDNS query until it times out.
            # See https://github.com/esp8266/Arduino/issues/2110 for details.
            # We monkey-patch getaddrinfo to look only for IPv4 addresses.
            socket.getaddrinfo = ipv4_getaddrinfo
    def _getcmd(self):
        """Helper method to handle multiple commands"""
        if not self.cmdlist: return None
        return self.cmdlist.pop(0)
        
    def _ungetcmd(self, cmd):
        """Helper method to handle multiple commands"""
        self.cmdlist.insert(0, cmd)
        
    def loadWifi(self):
        """Load WiFi network (if not already done)"""
        if self.wifi: return
        self.wifi = api.IotsaWifi()
        if self.args.ssid:
            ok = self.wifi.selectNetwork(self.args.ssid, self.args.ssidpw)
            if not ok:
                print("%s: cannot select wifi network %s" % (sys.argv[0], self.args.ssid), file=sys.stderr)
                sys.exit(1)
        return

    def loadDevice(self):
        """Load target device (if not already done)"""
        if self.device: return
        self.loadWifi()
        if not self.args.target or self.args.target == "auto":
            all = self.wifi.findDevices()
            if len(all) == 0:
                print("%s: no iotsa devices found" % (sys.argv[0]), file=sys.stderr)
                sys.exit(1)
            if len(all) > 1:
                print("%s: multiple iotsa devices:" % (sys.argv[0]), end=' ', file=sys.stderr)
                for a in all:
                    print(a, end=' ')
                print()
                sys.exit(1)
            self.args.target = all[0]
        if self.args.target:
            ok = self.wifi.selectDevice(self.args.target)
        self.device = api.IotsaDevice(self.wifi.currentDevice(), protocol=self.args.protocol, port=self.args.port, noverify=self.args.noverify)
        if self.args.bearer:
            self.device.setBearerToken(self.args.bearer)
        if self.args.credentials:
            self.device.setLogin(*self.args.credentials.split(':'))
        if not self.args.compat:
            self.device.load()
            
    def cmd_config(self):
        """Set target configuration parameters (target must be in configuration mode)"""
        self.loadDevice()
        if self.device.get('currentMode', 0) != 1 and not self.device.get('privateWifi', 0):
            raise api.UserIntervention("Set target into configuration mode first. See configMode or configWait commands.")
        
        anyDone = False
        while True:
            subCmd = self._getcmd()
            if not subCmd or subCmd == '--':
                break
            if not '=' in subCmd:
                self._ungetcmd(subCmd)
                break
            name, value = subCmd.split('=', 1)
            self.device.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: config: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        self.device.save()
        
    def cmd_configMode(self):
        """Ask target to go into configuration mode"""
        self.loadDevice()
        self.device.set('requestedMode', 1)
        self.device.save()

    def cmd_configWait(self):
        """Ask target to go into configuration mode and wait until it is (probably after user intervention)"""
        self._configModeAndWait(1)
        
    def cmd_factoryReset(self):
        """Ask device to do a factory-reset"""
        self.loadDevice()
        self.device.set('requestedMode', 3)
        self.device.save()
        
    def cmd_help(self):
        """List available commands"""
        for name in dir(self):
            if not name.startswith('cmd_'): continue
            handler = getattr(self, name)
            print('%-10s\t%s' % (name[4:], handler.__doc__))
            
    def cmd_info(self):
        """Show information on current target"""
        self.loadDevice()
        self.device.printStatus()

    def cmd_networks(self):
        """List iotsa wifi networks"""
        self.loadWifi()
        networks = self.wifi.findNetworks()
        for n in networks: print(n)
        
    def cmd_ota(self):
        """Upload new firmware to target (target must be in ota mode)"""
        filename = self._getcmd()
        if not filename:
            print("%s: ota requires a filename or URL" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        filename, _ = urllib.request.urlretrieve(filename)
        self.loadDevice()
        ESPOTA="~/.platformio/packages/tool-espotapy/espota.py"
        ESPOTA = os.environ.get("ESPOTA", ESPOTA)
        ESPOTA = os.path.expanduser(ESPOTA)
        if not os.path.exists(ESPOTA):
            print("%s: Not found: %s" % (sys.argv[0], ESPOTA), file=sys.stderr)
            print("%s: Please install espota.py and optionally set ESPOTA environment variable", file=sys.stderr)
            sys.exit(1)
        #cmd = [ESPOTA, '-i', self.device.ipAddress, '-f', filename]
        cmd = '"%s" -i %s -f "%s"' % (ESPOTA, self.device.ipAddress, filename)
        status = subprocess.call(cmd, shell=True)
        if status != 0:
            print("%s: OTA command %s failed" % (sys.argv[0], ESPOTA), file=sys.stderr)
            sys.exit(1)
        
        
    def cmd_otaMode(self):
        """Ask target to go into over-the-air programming mode"""
        self.loadDevice()
        self.device.set('requestedMode', 2)
        self.device.save()

    def cmd_otaWait(self):
        """Ask target to go into over-the-air programming mode and wait until it is (probably after user intervention)"""
        self._configModeAndWait(2)
        
    def cmd_targets(self):
        """List iotsa devices visible on current network"""
        self.loadWifi()
        targets = self.wifi.findDevices()
        for t in targets: print(t)
        
    def cmd_wifiInfo(self):
        """Show WiFi information for target"""
        self.loadDevice()
        wifi = self.device.getApi('wificonfig')
        wifi.load()
        wifi.printStatus()
        
    def cmd_wifiConfig(self):
        """Set WiFi parameters (target must be in configuration or private WiFi mode)"""
        self.loadDevice()
        wifi = self.device.getApi('wificonfig')
        wifi.load()
        
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
            wifi.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: wifiConfig: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        wifi.save()
        
    def cmd_xInfo(self):
        """Show target information for a specific module, next argument is module name"""
        self.loadDevice()
        modName = self._getcmd()
        if not modName:
            print("%s: xInfo requires a module name" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext = self.device.getApi(modName)
        ext.load()
        ext.printStatus()
        
    def cmd_xConfig(self):
        """Configure a specific module on the target, next argument is module name"""
        self.loadDevice()
        modName = self._getcmd()
        if not modName:
            print("%s: xConfig requires a module name" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext = self.device.getApi(modName)
        ext.load()
        
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
            ext.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: xConfig %s: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext.save()
        
def main():
    m = Main()
    m.run()
    
if __name__ == '__main__':
    main()
    
