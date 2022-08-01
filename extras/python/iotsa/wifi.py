import sys
import time
import os
import socket
from abc import ABC, abstractmethod
from typing import List, Optional

from .mdns import PlatformMDNSCollector

from .consts import UserIntervention, VERBOSE


class AbstractPlatformWifi(ABC):
    """Platform-dependent code for listing and joining of WiFi networks."""

    @abstractmethod
    def platformListWifiNetworks(self) -> List[str]:
        """Return list of network SSIDs that appear to be iotsa boards"""
        pass

    @abstractmethod
    def platformCurrentWifiNetworks(self) -> List[str]:
        """Return all currently connected wifi networks"""
        pass

    @abstractmethod
    def platformJoinWifiNetwork(self, ssid: str, password: str) -> bool:
        """Join a wifi network"""
        pass


if sys.platform == "darwin":
    import subprocess
    import plistlib

    class PlatformWifi(AbstractPlatformWifi):
        """MacOS WiFi handling"""

        def __init__(self):
            self.wifiInterface = os.getenv("IOTSA_WIFI")
            if not self.wifiInterface:
                cmd = "networksetup -listnetworkserviceorder"
                p = subprocess.Popen(
                    cmd, shell=True, stdout=subprocess.PIPE, universal_newlines=True
                )
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
            p = subprocess.Popen(
                "/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport --scan --xml",
                shell=True,
                stdout=subprocess.PIPE,
            )
            assert p.stdout
            data = plistlib.load(p.stdout, fmt=plistlib.FMT_XML)
            wifiNames = [d["SSID_STR"] for d in data]
            wifiNames.sort()
            if VERBOSE:
                print("Wifi networks found:", wifiNames)
            return wifiNames

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

        def platformCurrentWifiNetworks(self) -> List[str]:
            if VERBOSE:
                print("Find current networks (OSX)")
            cmd = "networksetup -getairportnetwork %s" % self.wifiInterface
            p = subprocess.Popen(
                cmd, shell=True, stdout=subprocess.PIPE, universal_newlines=True
            )
            assert p.stdout
            data = p.stdout.read()
            if VERBOSE:
                print("Find result was:", data)
            wifiName = data.split(":")[-1]
            wifiName = wifiName.strip()
            return [wifiName]

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

    def selectNetwork(self, ssid: str, password: str = "") -> bool:
        """Select a WiFi network"""
        if self._isNetworkSelected(ssid):
            return True
        ok = self.platformJoinWifiNetwork(ssid, password)
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
                return False
            except socket.gaierror:
                print("Unknown host: %s" % deviceName, file=sys.stderr)
                return False
            except socket.error:
                continue
            return True
        return False

    def findDevices(self) -> List[str]:
        """Return list of all iotsa devices visible on current network(s)"""
        if self._isConfigNetwork():
            if self._checkDevice("192.168.4.1"):
                return ["192.168.4.1"]
        collect = PlatformMDNSCollector()
        devices = collect.run()
        rv = []
        # Remove final dot (.) that can be appended (certificate matching doesn't like this)
        for d in devices:
            if d[-1:] == ".":
                rv.append(d[:-1])
            else:
                rv.append(d)
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
        return self.device
