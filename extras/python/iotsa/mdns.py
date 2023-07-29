import sys
import time
from abc import ABC, abstractmethod
from typing import List, Tuple, Dict, Any

from .consts import UserIntervention, VERBOSE

_have_mdns = sys.platform in ("darwin", "linux2", "linux")

class AbstractMDNSCollector(ABC):
    
    @abstractmethod
    def run(self, timeout: int = 5) -> List[Tuple[str, Dict[str, str]]]:
        ...

if _have_mdns:
    import zeroconf

    class ZeroconfMDNSCollector(AbstractMDNSCollector, zeroconf.ServiceListener):
        """Search for iotsa devices using mDNS/zeroconf/bonjour/rendezvous

        All devices advertising on mDNS will be checked for providing an _iotsa service
        (over http/https/tcp/coap) and only the devices that do will be returned.
        """

        found : List[tuple[str, Dict[str, str]]]

        def __init__(self):
            self.found = []
            if VERBOSE:
                print("Start mDNS browser for _iotsa._tcp.local.")
            self.zeroconf = zeroconf.Zeroconf()
            self.browsers = []
            self.browsers.append(
                zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._tcp.local.", self)
            )
            self.browsers.append(
                zeroconf.ServiceBrowser(self.zeroconf, "_iotsa._udp.local.", self)
            )

        def remove_service(self, zc, type, name):
            pass

        def update_service(self, zc, type, name):
            pass
            
        def add_service(self, zc : zeroconf.Zeroconf, type_ : str, name : str):
            if VERBOSE:
                print("Found mDNS entry for", name, "type:", type_)
            info = zc.get_service_info(type_, name)
            assert info
            if info.server in self.found:
                if VERBOSE:
                    print("Ignore duplicate mDNS entry for", info.server, "type:", type_)
                return
            properties : Dict[str, str] = {}
            for k, v in info.properties.items():
                try:
                    k = k.decode('utf8')
                except:
                    pass
                try:
                    v = v.decode('utf8')
                except:
                    pass
                properties[k] = v
            assert info.server
            self.found.append((info.server, properties))

        def run(self, timeout: int = 5) -> List[Tuple[str, Dict[str, str]]]:
            """Run the mDNS browser.

            :param timeout: how many seconds to wait for iotsa devices
            :return: list of iotsa devices found
            """
            time.sleep(timeout)
            if self.zeroconf:
                self.zeroconf.close()
            self.zeroconf = None
            self.browsers = []
            if VERBOSE:
                print("Stop mDNS browsing, found", self.found)
            return self.found

else:

    class DummyMDNSCollector(AbstractMDNSCollector):
        """Default mDNS handling: ask the user"""

        def __init__(self):
            pass

        def run(self, timeout=5) -> List[Tuple[str, Dict[str, str]]]:
            """Run for a short while (5 seconds default) and collect all iotsa devices found. Return list."""
            raise UserIntervention("Please browse for mDNS services _iotsa._tcp.local")

def PlatformMDNSCollector() -> AbstractMDNSCollector:
    if _have_mdns:
        return ZeroconfMDNSCollector()
    else:
        return DummyMDNSCollector()