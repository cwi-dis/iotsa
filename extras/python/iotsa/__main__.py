#!/usr/bin/env python
from __future__ import print_function
from __future__ import absolute_import
from __future__ import unicode_literals
from future import standard_library
standard_library.install_aliases()
from builtins import object
import argparse
import sys
import requests
import time
import os
import subprocess
import urllib.request, urllib.parse, urllib.error
import socket

from . import api
from . import consts

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
        self.dfu = None
        self.ble = None
        self.cmdlist = []
        
    def __del__(self):
        self.close()
        
    @classmethod
    def _helpinfo(cls):
        """Return available command help"""
        # Get attributes that refer to commands 
        names = dir(cls)
        names = filter(lambda x: x.startswith('cmd_'), names)
        names = sorted(names)
        rv = []
        for name in names:
            handler = getattr(cls, name)
            rv.append('%-10s\t%s' % (name[4:], handler.__doc__))
        return '\n'.join(rv)

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
                    print("%s: unknown command %s, --help for help" % (sys.argv[0], cmd), file=sys.stderr)
                    sys.exit(1)
                handler = getattr(self, cmdName)
                try:
                    handler()
                except api.UserIntervention as arg:
                    print("%s: %s: user intervention required:" % (sys.argv[0], cmd), file=sys.stderr)
                    print("%s: %s" % (sys.argv[0], arg), file=sys.stderr)
                    sys.exit(2)
                except api.IotsaError as arg:
                    print("%s: %s: IotsaError %s" % (sys.argv[0], cmd, arg), file=sys.stderr)
                    sys.exit(1)
                except requests.exceptions.HTTPError as arg:
                    print("%s: %s: HTTPError %s" % (sys.argv[0], cmd, arg), file=sys.stderr)
                    sys.exit(1)
                except api.CoapError as arg:
                    print("%s: %s: CoapError %s" % (sys.argv[0], cmd, arg), file=sys.stderr)
                    sys.exit(1)
        finally:
            self.close()

    def parseArgs(self):
        """Command line argument handling"""
        epilog = "Available commands:\n" + Main._helpinfo()
        parser = argparse.ArgumentParser(description="Change settings or update software on iotsa devices", epilog=epilog, formatter_class=argparse.RawDescriptionHelpFormatter)
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
        parser.add_argument('--noSystemRootCertificates', action="store_true", help='Do not use system root certificates, use REQUESTS_CA_BUNDLE or what requests package has')
        parser.add_argument("--compat", action="store_true", help="Compatability for old iotsa devices (ota only)")
        parser.add_argument("--serial", metavar="TTY", help="Serial port to use for DFU commands (default: automatically select)")
        parser.add_argument("command", nargs="+", help="Command to run")
        self.args = parser.parse_args()
        if self.args.verbose:
            consts.VERBOSE.append(True)
        if not self.args.ipv6:
            # Current esp8266 Arduino mDNS implementation has a problem: it replies with an IPv4 address but does not send a
            # negative reply for the IPv6 address. This causes requests to retry the mDNS query until it times out.
            # See https://github.com/esp8266/Arduino/issues/2110 for details.
            # We monkey-patch getaddrinfo to look only for IPv4 addresses.
            socket.getaddrinfo = ipv4_getaddrinfo
        if not self.args.noSystemRootCertificates and not os.environ.get('REQUESTS_CA_BUNDLE', None):
            # The requests package uses its own set of certificates, ignoring the ones the user has added to the system
            # set. By default, override that behaviour.
            for cf in ["/etc/ssl/certs/ca-certificates.crt", "/etc/ssl/certs/ca-certificates.crt"]:
                if os.path.exists(cf):
                    os.putenv('REQUESTS_CA_BUNDLE', cf)
                    os.environ['REQUESTS_CA_BUNDLE'] = cf
                    break

    def _getcmd(self):
        """Helper method to handle multiple commands"""
        if not self.cmdlist: return None
        return self.cmdlist.pop(0)
   
    def _getnamevalue(self, modName):
        """Helper method to return name=value or name=type:value arguments"""
        subCmd = self._getcmd()
        if not subCmd or subCmd == '--':
            return None, None
        if not '=' in subCmd:
            self._ungetcmd(subCmd)
            return None, None
        name, rest = subCmd.split('=')
        if type(rest) == type(()):
            value = '='.join(rest)
        else:
            value = rest
        if ':' in value:
            typename, rest = value.split(':')
            typecast = eval(typename)
            value = typecast(rest)
            print(f"{sys.argv[0]}: xConfig {modName}: {name}={value} after cast.", file=sys.stderr)
        return name, value

    def _ungetcmd(self, cmd):
        """Helper method to handle multiple commands"""
        self.cmdlist.insert(0, cmd)
        
    def loadDFU(self):
        """Load DFU driver (to allow flashing over USB or Serial Link for dead iotsa device)"""
        if self.dfu: return
        self.dfu = api.DFU(self.args.serial)
        self.dfu.dfuWait()

    def loadBLE(self, loadTarget=True):
        """Load Bluetooth LE driver"""
        if self.ble: return
        self.ble = api.BLE()
        if not loadTarget:
            return
        if not self.args.target or self.args.target == "auto":
            all = self.ble.findDevices()
            if len(all) == 0:
                print("%s: no iotsa BLE devices found" % (sys.argv[0]), file=sys.stderr)
                sys.exit(1)
            if len(all) > 1:
                print("%s: multiple iotsa BLE devices:" % (sys.argv[0]), end=' ', file=sys.stderr)
                for a in all:
                    print(a, end=' ', file=sys.stderr)
                print(file=sys.stderr)
                sys.exit(1)
            self.args.target = all[0]
        if self.args.target:
            ok = self.ble.selectDevice(self.args.target)
            if not ok:
                sys.exit(1)        
    
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
                    print(a, end=' ', file=sys.stderr)
                print(file=sys.stderr)
                sys.exit(1)
            self.args.target = all[0]
        if self.args.target:
            ok = self.wifi.selectDevice(self.args.target)
            if not ok:
                sys.exit(1)
        kwargs = {}
        if self.args.bearer:
            kwargs['bearer'] = self.args.bearer
        if self.args.credentials:
            kwargs['auth'] = self.args.credentials.split(':')
        self.device = api.IotsaDevice(self.wifi.currentDevice(), protocol=self.args.protocol, port=self.args.port, noverify=self.args.noverify, **kwargs)
            
    def cmd_config(self):
        """Set target configuration parameters (target must be in configuration mode)"""
        self.loadDevice()
        if self.device.config.get('currentMode', 0) != 1 and not self.device.config.get('privateWifi', 0):
            raise api.UserIntervention("Set target into configuration mode first. See configMode or configWait commands.")
        
        anyDone = False
        self.device.config.transaction()
        while True:
            name, value = self._getnamevalue("config")
            if not name:
                break
            self.device.config.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: config: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        self.device.config.commit()
        
    def cmd_configMode(self):
        """Ask target to go into configuration mode"""
        self.loadDevice()
        self.device.gotoMode('config', wait=False, verbose=True)

    def cmd_configWait(self):
        """Ask target to go into configuration mode and wait until it is (probably after user intervention)"""
        self.loadDevice()
        self.device.gotoMode('config', wait=True, verbose=True)
        
    def cmd_factoryReset(self):
        """Ask device to do a factory-reset"""
        self.loadDevice()
        self.device.gotoMode('factoryReset', wait=False, verbose=True)
            
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
        self.loadDevice()
        self.device.ota(filename)        
        
    def cmd_otaMode(self):
        """Ask target to go into over-the-air programming mode"""
        self.loadDevice()
        self.loadDevice()
        self.device.gotoMode('ota', wait=False, verbose=True)

    def cmd_otaWait(self):
        """Ask target to go into over-the-air programming mode and wait until it is (probably after user intervention)"""
        self.loadDevice()
        self.device.gotoMode('ota', wait=True, verbose=True)
        
    def cmd_dfuMode(self):
        """Check whether there is a target connected in DFU mode (via USB or serial port)"""
        self.loadDFU()
        
    def cmd_dfuClear(self):
        """Completely erase flash of target connected in DFU mode (via USB or serial port)"""
        self.loadDFU()
        self.dfu.dfuClear()
        
    def cmd_dfuLoad(self):
        """Load new firmware to target connected in DFU mode (via USB or serial port)"""
        filename = self._getcmd()
        if not filename:
            print("%s: dfuLoad requires a filename or URL" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        self.loadDFU()
        ok = self.dfu.dfuLoad(filename)
        ok = self.dfu.dfuRun()
        
    def cmd_bleTargets(self):
        """List iotsa devices accessible over BLE"""
        self.loadBLE(loadTarget=False)
        all = self.ble.findDevices()
        for d in all:
            print(d)

    def cmd_bleInfo(self):
        """Get information on BLE iotsa device"""
        self.loadBLE()
        print('xxxjack ble loaded')
        self.ble.printStatus()
        print('xxxjack status printed')

    def cmd_ble(self):
        """Get or set BLE characteristic on iotsa device"""
        self.loadBLE()
        subcommand = self._getcmd()
        if '=' in subcommand:
            # Set command
            name, value = subcommand.split("=")
            self.ble.set(name, value)
        else:
            # Get command
            value = self.ble.get(subcommand)
            print(value)

    def cmd_targets(self):
        """List iotsa devices visible on current network"""
        self.loadWifi()
        targets = self.wifi.findDevices()
        for t in targets: print(t)
        
    def cmd_allInfo(self):
        """Show all information for all modules for target"""
        self.loadDevice()
        all = self.device.getAll()
        self._printall(all, 0)

    def _printall(self, d, indent):
        if  isinstance(d, dict):
            print()
            for k, v in d.items():
                print('{}{}: '.format(' '*indent, k), end='')
                self._printall(v, indent+4)
        elif isinstance(d, list):
            print()
            for v in d:
                print('{}- '.format(' '*indent), end='')
                self._printall(v, indent+4)
        else:
            print(repr(d))

    def cmd_wifiInfo(self):
        """Show WiFi information for target"""
        self.loadDevice()
        wifi = self.device.getApi('wificonfig')
        wifi.printStatus()
        
    def cmd_wifiConfig(self):
        """Set WiFi parameters (target must be in configuration or private WiFi mode)
        Parameters are name=value, or optionally name=type:value for example name=int:0"""
        self.loadDevice()
        wifi = self.device.getApi('wificonfig')
        wifi.transaction()
        anyDone = False
        while True:
            name, value = self._getnamevalue("wifiConfig")
            if not name:
                break
            wifi.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: wifiConfig: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        wifi.commit()
        
    def cmd_xInfo(self):
        """Show target information for a specific module, next argument is module name"""
        self.loadDevice()
        modName = self._getcmd()
        if not modName:
            print("%s: xInfo requires a module name" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext = self.device.getApi(modName)
        ext.printStatus()
        
    def cmd_xConfig(self):
        """Configure a specific module on the target, next argument is module name
        Parameters are name=value, or optionally name=type:value for example name=int:0"""
        self.loadDevice()
        modName = self._getcmd()
        if not modName:
            print("%s: xConfig requires a module name" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext = self.device.getApi(modName)
        ext.transaction()
        anyDone = False
        while True:
            name, value = self._getnamevalue(modName)
            if not name:
                break
            ext.set(name, value)
            anyDone = True
        if not anyDone:
            print("%s: xConfig %s: requires name=value [...] to set config variables" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        ext.commit()

    def cmd_reboot(self):
        """Reboot the target"""
        self.loadDevice()
        self.device.reboot()
        
    def cmd_certificate(self):
        """Install https certificate. Arguments are keyfile and certfile (in PEM or DER)"""
        keyFilename = self._getcmd()
        certFilename = self._getcmd()
        if not keyFilename or not certFilename:
            print("%s: certificate requires keyfile and certfile arguments" % sys.argv[0], file=sys.stderr)
            sys.exit(1)
        if keyFilename[-4:] == '.pem':
            key = open(keyFilename, 'r').read()
        elif keyFilename[-4:] == '.der':
            key = open(keyFilename, 'rb').read()
        else:
            print("%s: certificate: keyfile must be .pem or .der" % sys.argv[0], file=sys.stderr)
        if certFilename[-4:] == '.pem':
            cert = open(certFilename, 'r').read()
        elif certFilename[-4:] == '.der':
            cert = open(certFilename, 'rb').read()
        else:
            print("%s: certificate: certfile must be .pem or .der" % sys.argv[0], file=sys.stderr)
        self.loadDevice()
        self.device.uploadCertificate(key, cert)
        
def main():
    m = Main()
    m.run()
    
if __name__ == '__main__':
    main()
    
