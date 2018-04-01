import sys
import time
import os

VERBOSE=False

if sys.platform == 'darwin':
    import subprocess
    import plistlib
    class PlatformWifi:
        def __init__(self):
            self.wifiInterface = os.getenv('IOTSA_WIFI', 'en2')

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

if sys.platform in ('darwin', 'linux2'):
    import zeroconf
    
    class PlatformMDNSCollector:
        def __init__(self):
            self.found = []
            if VERBOSE: print 'Start mDNS browser for _iotsa._tcp.local.'
            self.zeroconf = zeroconf.Zeroconf()
            self.browser = zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._tcp.local.", self)
        
        def remove_service(self, zc, type, name):
            pass
        
        def add_service(self, zc, type, name):
            if VERBOSE: print 'Found mDNS entry for', name, 'type:', type
            info = zc.get_service_info(type, name)
            if info.port != 80:
                print >>sys.stderr, "%s: Ignore %s with port %s" % (sys.argv[0], name, info.port)
                return
            self.found.append(info.server)
        
        def run(self, timeout=5):
            time.sleep(timeout)
            self.zeroconf.close()
            self.zeroconf = None
            self.browser = None
            if VERBOSE: print 'Stop mDNS browsing, found', self.found
            return self.found
        
