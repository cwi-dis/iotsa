import sys
import time
import os
import socket
from abc import ABC, abstractmethod
from typing import List, Optional, Dict, Any

from .mdns import PlatformMDNSCollector

from .consts import UserIntervention, VERBOSE

# Fixed IP address of the config AP hosted by an unconfigured/factory-mode iotsa device.
IOTSA_CONFIG_AP_IP = "192.168.4.1"


class AbstractPlatformWifi(ABC):
    """Platform-dependent code for listing and joining of WiFi networks."""

    @abstractmethod
    def platformListWifiNetworks(self) -> List[str]:
        """Return list of network SSIDs that appear to be iotsa boards"""
        pass

    @abstractmethod
    def platformCurrentWifiNetworks(self) -> List[str]:
        """Return all currently connected (fully bound, not mid-join) wifi networks"""
        pass

    @abstractmethod
    def platformCurrentGateway(self) -> Optional[str]:
        """Return the WiFi interface's current default gateway IP, or None if unknown/not connected"""
        pass

    @abstractmethod
    def platformJoinWifiNetwork(self, ssid: str, password: str) -> bool:
        """Join a wifi network"""
        pass

    @abstractmethod
    def platformSafeToSwitchWifi(self) -> Optional[bool]:
        """Would switching WiFi drop the only route to the internet?

        Returns True if there is another route (safe to switch), False if
        WiFi is confirmed to be the only route (unsafe), or None if this
        platform doesn't know how to check (treat as unsafe, require --force).
        """
        pass


if sys.platform == "darwin":
    import subprocess

    class PlatformWifi(AbstractPlatformWifi):
        """MacOS WiFi handling"""

        def __init__(self):
            self.wifiInterface = os.getenv("IOTSA_WIFI")
            if not self.wifiInterface:
                cmd = "networksetup -listnetworkserviceorder"
                p = subprocess.Popen(
                    cmd, shell=True, stdout=subprocess.PIPE, universal_newlines=True
                )
                assert p.stdout
                data = p.stdout.readlines()
                for line in data:
                    line = line.strip()
                    searchString = "(Hardware Port: Wi-Fi, Device: "
                    if line[: len(searchString)] == searchString and line[-1:] == ")":
                        self.wifiInterface = line[len(searchString) : -1]
                        if VERBOSE:
                            print("Using WiFi interface", self.wifiInterface)
                        break
                    else:
                        self.wifiInterface = "en1"

        def platformListWifiNetworks(self) -> List[str]:
            if VERBOSE:
                print("Listing wifi networks (OSX)")
            wifiNames = self._scanWithCoreWLAN()
            if wifiNames is None:
                wifiNames = self._scanWithWifiCli()
            if wifiNames is None:
                raise UserIntervention(
                    "WiFi scan returned no results.\n"
                    "CoreWLAN requires Location Services permission for Terminal or Python\n"
                    "(System Settings > Privacy & Security > Location Services).\n"
                    "Alternatively, install wifi-cli: brew install rgeraskin/homebrew/wifi-cli"
                )
            if VERBOSE:
                print("Wifi networks found:", wifiNames)
            return wifiNames

        def _scanWithCoreWLAN(self) -> Optional[List[str]]:
            """Try CoreWLAN scan. Returns list of SSIDs, or None if location permission is missing."""
            import CoreWLAN
            iface = CoreWLAN.CWWiFiClient.sharedWiFiClient().interface()
            networks, err = iface.scanForNetworksWithName_error_(None, None)
            if err:
                return None
            # Without location permission, networks are returned but with null SSIDs
            result = sorted(set(n.ssid() for n in networks if n.ssid()))
            if not result:
                return None
            return result

        def _scanWithWifiCli(self) -> Optional[List[str]]:
            """Try wifi-cli scan. Returns list of SSIDs, or None if wifi-cli is not installed."""
            import json
            import shutil
            if not shutil.which("wifi-cli"):
                return None
            p = subprocess.run(
                ["wifi-cli", "--json", "scan"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if p.returncode != 0:
                return None
            data = json.loads(p.stdout)
            return sorted(entry["ssid"] for entry in data if "ssid" in entry)

        def platformJoinWifiNetwork(self, ssid: str, password: str) -> bool:
            if VERBOSE:
                print("Joining network (OSX):", ssid)
            cmd = "networksetup -setairportnetwork %s %s" % (self.wifiInterface, ssid)
            if password:
                cmd += " " + password
            status = p = subprocess.call(cmd, shell=True)
            if VERBOSE:
                print("Join network status:", status)
            return status == 0

        def _getIpconfigStatus(self) -> Optional[Dict[str, str]]:
            """Return {'state', 'ssid', 'router'} for the WiFi interface via `ipconfig getsummary`.

            This is more reliable than `networksetup -getairportnetwork`, which has been
            observed to report "not associated" while actually connected. `state` is
            "BOUND" only once DHCP has actually completed, not merely associated at L2 —
            that's the signal to poll for after a join.
            """
            try:
                p = subprocess.run(
                    ["ipconfig", "getsummary", self.wifiInterface],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    timeout=5,
                )
            except (OSError, subprocess.TimeoutExpired):
                return None
            if p.returncode != 0:
                return None
            result: Dict[str, str] = {}
            for line in p.stdout.splitlines():
                line = line.strip()
                for key, field in (("State :", "state"), ("SSID :", "ssid"), ("Router :", "router")):
                    if line.startswith(key):
                        result[field] = line[len(key):].strip()
            return result or None

        def platformCurrentWifiNetworks(self) -> List[str]:
            if VERBOSE:
                print("Find current networks (OSX)")
            status = self._getIpconfigStatus()
            if VERBOSE:
                print("ipconfig getsummary result was:", status)
            if not status or status.get("state") != "BOUND":
                return []
            ssid = status.get("ssid")
            return [ssid] if ssid else []

        def platformCurrentGateway(self) -> Optional[str]:
            status = self._getIpconfigStatus()
            if not status or status.get("state") != "BOUND":
                return None
            return status.get("router")

        def platformSafeToSwitchWifi(self) -> Optional[bool]:
            # Look up which interface the OS would actually use to reach the
            # internet (a plain routing-table lookup, no packet is sent). If
            # it's not our WiFi interface, some other route exists and it's
            # safe to switch WiFi away.
            try:
                p = subprocess.run(
                    ["route", "get", "1.1.1.1"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True,
                    timeout=5,
                )
            except (OSError, subprocess.TimeoutExpired):
                return None
            if p.returncode != 0:
                return None
            routeInterface = None
            for line in p.stdout.splitlines():
                line = line.strip()
                if line.startswith("interface:"):
                    routeInterface = line.split(":", 1)[1].strip()
                    break
            if routeInterface is None:
                return None
            if VERBOSE:
                print("Route to internet uses interface", routeInterface, "WiFi interface is", self.wifiInterface)
            return routeInterface != self.wifiInterface

else:

    class PlatformWifi(AbstractPlatformWifi):
        """Default WiFi handling: ask the user."""

        def __init__(self):
            pass

        def platformListWifiNetworks(self) -> List[str]:
            raise UserIntervention(
                "Please look for WiFi SSIDs (network names) starting with 'config-'"
            )

        def platformJoinWifiNetwork(self, ssid: str, password: str) -> bool:
            raise UserIntervention("Please join WiFi network named %s" % ssid)

        def platformCurrentWifiNetworks(self) -> List[str]:
            return []

        def platformCurrentGateway(self) -> Optional[str]:
            return None

        def platformSafeToSwitchWifi(self) -> Optional[bool]:
            # We don't know how to check this on this platform.
            return None


class IotsaWifi(PlatformWifi):
    """Handle WiFi networks for contacting iotsa devices"""

    def __init__(self):
        PlatformWifi.__init__(self)
        self.ssid = None
        self.device = None

    def findNetworks(self) -> List[str]:
        """Returns list of all WiFi SSIDs that may be unconfigured iotsa devices"""
        all = self.platformListWifiNetworks()
        rv = []
        for net in all:
            if net.startswith("config-"):
                rv.append(net)
        return rv

    def _isNetworkSelected(self, ssid: str) -> bool:
        """Is this wifi network selected?"""
        return ssid in self.platformCurrentWifiNetworks()

    def _isConfigNetwork(self) -> bool:
        """Is the currently selected wifi network hosted by an unconfigured iotsa device?"""
        if self.ssid and self.ssid.startswith("config-"):
            return True
        for c in self.platformCurrentWifiNetworks():
            if c.startswith("config-"):
                return True
        return False

    def selectNetwork(self, ssid: str, password: str = "", timeoutSeconds: float = 15, pollInterval: float = 0.5) -> bool:
        """Select a WiFi network"""
        ok = self._isNetworkSelected(ssid)
        if not ok:
            ok = self.platformJoinWifiNetwork(ssid, password)
            if ok:
                # The join call can return before the OS has actually finished
                # joining (and networksetup returns 0 even when the SSID isn't
                # visible at all), so poll until we're really connected or we
                # time out.
                deadline = time.monotonic() + timeoutSeconds
                ok = self._isNetworkSelected(ssid)
                while not ok and time.monotonic() < deadline:
                    time.sleep(pollInterval)
                    ok = self._isNetworkSelected(ssid)
        if ok and ssid.startswith("config-"):
            # Extra confirmation for iotsa config APs, which always hand out this
            # fixed gateway: guards against having silently fallen back to a
            # different (e.g. the normal home) network with the same connected state.
            gateway = self.platformCurrentGateway()
            if gateway is not None and gateway != IOTSA_CONFIG_AP_IP:
                ok = False
        if ok:
            self.ssid = ssid
        return ok

    def _checkDevice(self, deviceName: str) -> bool:
        """Is the given host a possible iotsa device?"""
        ports = [80, 443]
        for port in ports:
            try:
                _ = socket.create_connection((deviceName, port), 20)
            except socket.timeout:
                print(f"Timeout: {deviceName}:{port}", file=sys.stderr)
                return False
            except socket.gaierror:
                print(f"Unknown host: {deviceName}", file=sys.stderr)
                return False
            except socket.error:
                continue
            return True
        print(f"Neither http nor https responding: {deviceName}", file=sys.stderr)
        return False

    def findDevices(self) -> List[tuple[str, Dict[str, str]]]:
        """Return list of all iotsa devices visible on current network(s)"""
        if self._isConfigNetwork():
            if self._checkDevice(IOTSA_CONFIG_AP_IP):
                return [(IOTSA_CONFIG_AP_IP, {})]
        collect = PlatformMDNSCollector()
        devices = collect.run()
        rv : List[tuple[str, Dict[str, str]]] = []
        # Remove final dot (.) that can be appended (certificate matching doesn't like this)
        for name, properties in devices:
            if name[-1:] == ".":
                rv.append((name[:-1], properties))
            else:
                rv.append((name, properties))
        return rv

    def selectDevice(self, device: str) -> bool:
        """Select a iotsa device"""
        if not "." in device:
            device = device + ".local"
        if self._checkDevice(device):
            self.device = device
            return True
        return False

    def currentDevice(self) -> str:
        """Return the currently selected iotsa device"""
        assert self.device
        return self.device
