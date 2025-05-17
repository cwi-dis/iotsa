import sys
import os
import subprocess
import copy
import time
import binascii
import io
import urllib.request
import requests.exceptions
from typing import Any, Optional, Tuple, Union

from .consts import UserIntervention, IotsaError, CoapError, VERBOSE
from .dfu import DFU
from .ble import BLE
from .wifi import IotsaWifi
from .protocols import HandlerForProto, IotsaAbstractProtocolHandler


class IotsaEndpoint:
    """Class representing a iotsa (REST or COAP) endpoint.

    :param device: The iotsa device to which this endpoint belongs
    :param api: string used to address the endpoint within the device
    :param cache: if true allow values to be reused without contacting the device again
    """
    device : "IotsaDevice"
    endpoint : str
    status : dict[str, Any]
    settings : dict[str, Any]
    cache : bool
    didLoad : bool
    inTransaction : bool

    def __init__(self, device: "IotsaDevice", api: str, cache: bool = False):
        self.device = device
        self.endpoint = api
        self.status = {}
        self.settings = {}
        self.cache = cache
        self.didLoad = False
        self.inTransaction = False

    def __repr__(self) -> str:
        return f"IotsaEndpoint(endpoint={repr(self.endpoint)},device={repr(self.device)})"
    def load(self) -> None:
        """Load all data from the endpoint (unless cached data is available)"""
        if self.didLoad and self.cache:
            return
        self.status = {}
        self.didload = False
        assert self.device.protocolHandler
        self.status = self.device.protocolHandler.get(self.endpoint)
        if self.status == "" or self.status == None:
            print(f"Warning: GET {self.endpoint} returned empty response")
            self.status = {}
        self.didLoad = True

    def flush(self) -> None:
        """Clear all loaded data (so it will be reloaded even when caching)"""
        self.didLoad = False
        self.status = {}
        self.settings = {}

    def transaction(self) -> None:
        """Start set of updates that will be sent to the device atomically"""
        self.inTransaction = True

    def commit(self) -> None:
        """Send all updates since transaction() to the device"""
        assert self.device.protocolHandler
        settings = self.settings
        self.settings = {}
        self.inTransaction = False
        if settings:
            self.device.flush()
            reply = self.device.protocolHandler.put(self.endpoint, json=settings)
            if reply and reply.get("needsReboot"):
                if "requestedModeTimeout" in reply:
                    msg = "Reboot %s within %s seconds to activate mode %s" % (
                        self.device.ipAddress,
                        reply.get("requestedModeTimeout", "???"),
                        self.device.modeName(reply.get("requestedMode")),
                    )
                    raise UserIntervention(msg)
                if VERBOSE:
                    print("config: reboot to activate new setting")

    def get(self, name: str, default: Any = "no default"):
        """Get a named value from previous loaded (or set) data

        :param name: the name of the value to get
        :param default: optional default value, if not specified KeyError is raised
        """
        self.load()
        if default == "no default":
            return self.status[name]
        return self.status.get(name, default)

    def __getattr__(self, name):
        return self.get(name)

    def getAll(self) -> dict[str, Any]:
        """Return dictionary with all values"""
        self.load()
        rv = copy.deepcopy(self.status)
        return rv

    def set(self, name: str, value: Any) -> None:
        """Set a value immedeately (if not in a transaction) or remember the set operation for the commit

        :param name: name to set
        :param value: the value
        """
        self.settings[name] = value
        if not self.inTransaction:
            self.commit()

    def printStatus(self) -> None:
        """Print all names and values previously loaded"""
        self.load()
        print("%s:" % self.device.ipAddress)
        print("  %s:" % self.endpoint)
        for k, v in list(self.status.items()):
            print("    %-16s %s" % (str(k) + ":", v))


class IotsaDevice:
    """Class representing a iotsa device

    :param ipAddress: hostname or ip address of iotsa device
    :param port: (optional) override of port implied by protocol
    :param protocol: (optional) protocol to contact device on, default is to sniff the device
    :param noverify: do not verify HTTPS certificates (debug only)
    :param bearer: (optional) Authorization bearer token
    :param auth: (optional) Anthentication tuple
    """
    ipAddress : str
    protocolHandler : Optional[IotsaAbstractProtocolHandler]
    auth : Optional[Tuple[str, str]]
    bearerToken : Optional[str]

    def __init__(
        self,
        ipAddress: str,
        port: Optional[int] = None,
        protocol: Optional[str] = None,
        noverify: bool = False,
        bearer: Optional[str] = None,
        auth: Optional[Tuple[str, str]] = None,
    ):
        self.ipAddress = ipAddress
        if protocol == None:
            protocol, noverify = self._guessProtocol(ipAddress, port)
        url = "%s://%s" % (protocol, ipAddress)
        if port:
            url += ":%d" % port
        assert protocol
        try:
            HandlerClass = HandlerForProto[protocol]
        except KeyError:
            raise IotsaError(f"Unknown protocol: {protocol}")
        self.protocolHandler = HandlerClass(
            url, noverify=noverify, bearer=bearer, auth=auth
        )
        self.config = IotsaEndpoint(self, "config", cache=True)
        self.auth = None
        self.bearerToken = None
        self.apis = {"config": self.config}

    _ProtocolCache: dict[Tuple[str, Optional[int]], Tuple[str, bool]] = {}

    @classmethod
    def _guessProtocol(klass, ipAddress: str, port: Optional[int]) -> Tuple[str, bool]:
        """Sniff device to guess supported protocol"""
        if (ipAddress, port) in klass._ProtocolCache:
            return klass._ProtocolCache[(ipAddress, port)]
        protocol = "https"
        noverify = False
        url = "%s://%s" % (protocol, ipAddress)
        if port:
            url += ":%d" % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.request("GET", "config", retryCount=0)
        except requests.exceptions.SSLError:
            if VERBOSE:
                print("Using https protocol with --noverify")
            rv = ("https", True)
            klass._ProtocolCache[(ipAddress, port)] = rv
            return rv
        except:
            pass
        else:
            if VERBOSE:
                print("Using https protocol")
            rv = ("https", False)
            klass._ProtocolCache[(ipAddress, port)] = rv
            return rv

        protocol = "http"
        url = "%s://%s" % (protocol, ipAddress)
        if port:
            url += ":%d" % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.request("GET", "config", retryCount=0)
        except:
            pass
        else:
            if VERBOSE:
                print("Using http protocol")
            rv = ("http", False)
            klass._ProtocolCache[(ipAddress, port)] = rv
            return rv

        protocol = "coap"
        noverify = True
        url = "%s://%s" % (protocol, ipAddress)
        if port:
            url += ":%d" % port
        HandlerClass = HandlerForProto[protocol]
        ph = HandlerClass(url, noverify=noverify)
        try:
            ph.get("config")
        except:
            pass
        else:
            if VERBOSE:
                print("Using coap protocol")
            rv = ("coap", False)
            klass._ProtocolCache[(ipAddress, port)] = rv
            return rv

        raise IotsaError(
            "Cannot determine protocol to use for {}, use --protocol".format(ipAddress)
        )

    def close(self) -> None:
        """Close connection to the iotsa device"""
        if self.protocolHandler:
            self.protocolHandler.close()
        self.protocolHandler = None
        self.apis = {}

    def flush(self) -> None:
        """Clear all endpoint caches"""
        for a in self.apis.values():
            a.flush()

    def setLogin(self, username: str, password: str) -> None:
        """Supply Basic authentication information"""
        self.auth = (username, password)

    def setBearerToken(self, token: str) -> None:
        """Supply Authorization bearer token"""
        self.bearerToken = token

    def getApi(self, api: str) -> IotsaEndpoint:
        """Return IotsaEndpoint for a given module within the device"""
        if not api in self.apis:
            self.apis[api] = IotsaEndpoint(self, api)
        return self.apis[api]

    def __getattr__(self, name):
        return self.getApi(name)

    def getAll(self) -> dict[str, Any]:
        """Get dictionary of all modules (except config, which is already in self.config)"""
        all = self.config.getAll()
        moduleNames = all.get("modules", [])
        modules = {}
        for m in moduleNames:
            if m == "config":
                continue
            api = self.getApi(m)
            modData = None
            try:
                modData = api.getAll()
            except IotsaError as arg:
                print(f"getAll: module {m}: {arg}", file=sys.stderr)
            modules[m] = modData
        all["modules"] = modules
        return all

    def printStatus(self):
        """Print all config settings and their values"""
        status = self.config.getAll()
        print("%s:" % self.ipAddress)
        print("  program:           ", status.pop("program", "unknown"))
        print("  last boot:         ", end=" ")
        lastboot = status.pop("uptime")
        if not lastboot:
            print("???", end=" ")
        else:
            if lastboot <= 60:
                print("%ds" % lastboot, end=" ")
            else:
                lastboot /= 60
                if lastboot <= 60:
                    print("%dm" % lastboot, end=" ")
                else:
                    lastboot /= 60
                    if lastboot <= 24:
                        print("%dh" % lastboot, end=" ")
                    else:
                        lastboot /= 24
                        print("%dd" % lastboot, end=" ")
        lastreason = status.pop("bootCause")
        if lastreason:
            print("(%s)" % lastreason, end=" ")
        print()
        print(
            "  runmode:           ", self.modeName(status.pop("currentMode", 0)), end=" "
        )
        timeout = status.pop("currentModeTimeout", None)
        if timeout:
            print("(%d seconds more)" % timeout, end=" ")
        print()
        if status.pop("privateWifi", False):
            print("     NOTE:         on private WiFi network")
        reqMode = status.pop("requestedMode", None)
        if reqMode:
            print("  requested mode:    ", self.modeName(reqMode), end=" ")
            timeout = status.pop("requestedModeTimeout", "???")
            if timeout:
                print("(needs reboot within %s seconds)" % timeout, end=" ")
            print()
        print("  hostName:          ", status.pop("hostName", ""))
        print("  modules:           ", end=" ")
        for m in status.pop("modules", ["???"]):
            print(m, end=" ")
        print()
        print("  features:          ", end=" ")
        for m in status.pop("features", ["???"]):
            print(m, end=" ")
        print()
        for k, v in list(status.items()):
            print("  %-19s %s" % (k + ":", v))

    def modeName(self, mode: int) -> str:
        """Convert iotsa runtime mode integer to descriptive string"""
        if mode is None:
            return "unknown"
        mode = int(mode)
        names = ["normal", "config", "ota", "factoryReset"]
        if mode >= 0 and mode < len(names):
            return names[mode]
        return "unknown-mode-%d" % mode

    def modeForName(self, name: str) -> int:
        """Convert iotsa runtime mode string to integer"""
        names = ["normal", "config", "ota", "factoryReset"]
        return names.index(name)

    def gotoMode(
        self, modeName: str, wait: bool = False, verbose: bool = False
    ) -> None:
        """Ask iotsa device to switch runtime mode

        Note that mode switching may require user intervention, such as rebooting the device.

        :param modeName: the runtime mode wanted
        :param wait: (optional) if True wait for the mode change to have happened
        :param verbose: print what is happening, and prompt the user if action is required
        """
        mode = self.modeForName(modeName)
        if self.config.get("currentMode") == mode:
            if verbose:
                print(
                    "%s: target already in mode %s" % (sys.argv[0], self.modeName(mode))
                )
            return
        self.config.transaction()
        self.config.set("requestedMode", mode)
        try:
            self.config.commit()
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
            if self.config.get("currentMode") == mode:
                break
            reqMode = self.config.get("requestedMode", 0)
            if self.config.get("requestedMode") != mode:
                raise IotsaError(
                    "target now has requestedMode %s in stead of %s?"
                    % (self.modeName(reqMode), self.modeName(mode))
                )
            if verbose:
                print(
                    "%s: Reboot %s within %s seconds to activate mode %s"
                    % (
                        sys.argv[0],
                        self.ipAddress,
                        self.config.get("requestedModeTimeout", "???"),
                        self.modeName(reqMode),
                    ),
                    file=sys.stderr,
                )
        if verbose:
            print("%s: target is now in %s mode" % (sys.argv[0], self.modeName(mode)))

    def reboot(self):
        """Reboot the iotsa device"""
        assert self.protocolHandler
        self.protocolHandler.put("config", json={"reboot": True})

    def _find_espota(self):
        if "ESPOTA" in os.environ:
            espota = os.environ["ESPOTA"]
            espota = os.path.expanduser(espota)
        else:
            candidates = [
                "~/.platformio/packages/tool-espotapy/espota.py",
                "~/.platformio/packages/framework-arduinoespressif8266/tools/espota.py",
            ]
            for path in candidates:
                espota = os.path.expanduser(path)
                if os.path.exists(espota):
                    break
        if not os.path.exists(espota):
            raise IotsaError(
                "Helper command not found: %s\nPlease install espota.py and optionally set ESPOTA environment variable"
                % (espota)
            )
        return espota

    def ota(self, filename: str) -> None:
        """Upload new firmware (Over The Air) to iotsa device

        Note that device must be in runtime mode ota for this call to succeed.
        """
        if not os.path.exists(filename):
            filename, _ = urllib.request.urlretrieve(filename)
        espota = self._find_espota()
        # cmd = ['python', espota, '-i', self.ipAddress, '-f', filename]
        cmd = 'python3 "%s" -i %s -f "%s"' % (espota, self.ipAddress, filename)
        status = subprocess.call(cmd, shell=True)
        if status != 0:
            raise IotsaError("OTA command %s failed" % (cmd))

    def uploadCertificate(
        self, keyData: Union[str, bytes], certificateData: Union[str, bytes]
    ) -> None:
        """Upload new SSL key and certificate to the device

        Note that device must be in mode config for this call to succeed
        """
        assert self.protocolHandler
        if isinstance(keyData, str) and keyData.startswith("---"):
            keyDataSplit = keyData.splitlines()
            keyDataSplit = keyDataSplit[1:-1]
            keyDataJoin = "\n".join(keyDataSplit)
            keyData = binascii.a2b_base64(keyDataJoin)
        assert isinstance(keyData, bytes)
        if isinstance(certificateData, str) and certificateData.startswith("---"):
            certificateDataSplit = certificateData.splitlines()
            certificateDataSplit = certificateDataSplit[1:-1]
            certificateDataJoin = "\n".join(certificateDataSplit)
            certificateData = binascii.a2b_base64(certificateDataJoin)
        assert isinstance(certificateData, bytes)
        keyFile = io.BytesIO(keyData)
        files = {
            "keyFile": ("httpsKey.der", keyFile, "application/binary"),
        }
        self.protocolHandler.post("/configupload", files=files)
        certificateFile = io.BytesIO(certificateData)
        files = {"certfile": ("httpsCert.der", certificateFile, "application/binary")}
        self.protocolHandler.post("/configupload", files=files)
