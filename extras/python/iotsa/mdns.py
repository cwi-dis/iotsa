import sys
import time

from .consts import UserIntervention, VERBOSE

class PlatformMDNSCollector(object):
    """Default mDNS handling: ask the user"""
    def __init__(self):
        pass
        
    def run(self, timeout=5):
        """Run for a short while (5 seconds default) and collect all iotsa devices found. Return list."""
        raise UserIntervention("Please browse for mDNS services _iotsa._tcp.local")
 
if sys.platform in ('darwin', 'linux2', 'linux'):
    import zeroconf
    
    class PlatformMDNSCollector(object):
        def __init__(self):
            self.found = []
            if VERBOSE: print('Start mDNS browser for _iotsa._tcp.local.')
            self.zeroconf = zeroconf.Zeroconf()
            self.browsers = []
            self.browsers.append(zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._tcp.local.", self))
            self.browsers.append(zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._http._tcp.local.", self))
            self.browsers.append(zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._https._tcp.local.", self))
            self.browsers.append(zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._coap._tcp.local.", self))
        
        def remove_service(self, zc, type, name):
            pass
        
        def add_service(self, zc, type, name):
            if VERBOSE: print('Found mDNS entry for', name, 'type:', type)
            info = zc.get_service_info(type, name)
            if info.server in self.found:
                if VERBOSE:
                    print('Ignore duplicate mDNS entry for', info.server, 'type:', type)
                return
            self.found.append(info.server)
        
        def run(self, timeout=5):
            time.sleep(timeout)
            self.zeroconf.close()
            self.zeroconf = None
            self.browsers = []
            if VERBOSE: print('Stop mDNS browsing, found', self.found)
            return self.found
        
