import ssl
import time
from typing import Optional, Tuple
import requests

from ..consts import VERBOSE
from .abstract import IotsaAbstractProtocolHandler, apiErrorMessage, checkApiReply

# We disable warnings about invalid certificates: we expect them and they provide no meaningful information
import urllib3
import urllib3.exceptions

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

class IotsaRESTProtocolHandler(IotsaAbstractProtocolHandler):
    """Communicate with iotsa device using REST over HTTP or HTTPS

    :param baseurl: first part of URL (endpoint arguments will be appended)
    :param noverify: skip SSL certificate verification (debugging only)
    :param bearer: (optional) Authorization bearer token
    :param auth: (optional) Anthentication tuple
    """

    def __init__(
        self,
        baseURL: str,
        noverify: bool = False,
        bearer: Optional[str] = None,
        auth: Optional[Tuple[str, str]] = None,
    ):
        if baseURL[-1] != "/":
            baseURL += "/"
        self.baseURL = baseURL
        self.noverify = noverify
        self.bearer = bearer
        self.auth = auth
        # One session per device, so a keep-alive connection is reused: a TLS
        # handshake takes seconds on an ESP32-C3 (cwi-dis/iotsa#293).
        self.session = requests.Session()

    def close(self):
        self.session.close()

    def get(self, endpoint, json=None):
        return self.request("GET", endpoint, json=json)

    def put(self, endpoint, json=None):
        return self.request("PUT", endpoint, json=json)

    def post(self, endpoint, json=None, files=None):
        assert files is None or json is None
        return self.request("POST", endpoint, json=json, files=files)

    def request(self, method, endpoint, json=None, files=None, retryCount=5):
        headers = {}
        if self.bearer:
            headers["Authorization"] = "Bearer " + self.bearer
        if endpoint[:1] == "/":
            url = self.baseURL + endpoint[1:]
        else:
            url = self.baseURL + "api/" + endpoint
        if VERBOSE:
            print("REST %s %s" % (method, url))
            if self.auth:
                print("auth", self.auth)
            if headers:
                print("....", headers)
            if json:
                print(">>>>", json)
        while True:
            try:
                r = self.session.request(
                    method,
                    url,
                    auth=self.auth,
                    json=json,
                    verify=not self.noverify,
                    headers=headers,
                    files=files,
                )
            except requests.exceptions.ConnectionError as e:
                # A rejected certificate won't get better. Other errors, also an
                # SSLError from a handshake the device broke off (out of memory,
                # cwi-dis/iotsa#293), or a reused connection the device closed
                # just as we sent, are worth a retry.
                if _isCertificateError(e):
                    raise
                if retryCount > 0:
                    if VERBOSE:
                        print("REST %s %s: %s, retrying" % (method, url, e))
                    retryCount -= 1
                    time.sleep(2)
                else:
                    raise
            else:
                break
        if r.history:
            print("Note: received redirect when accessing", url)
        if VERBOSE:
            print("<<<< status=%s reply=%s" % (r.status_code, r.text))
        if r.status_code >= 400:
            msg = apiErrorMessage(r.text)
            if msg:
                # Still an HTTPError (callers check e.response.status_code), with the device's reason.
                raise requests.exceptions.HTTPError(f"{r.status_code} {msg} ({method} {url})", response=r)
        r.raise_for_status()
        if r.text and r.text[0] == "{":
            return checkApiReply(r.json(), f"{method} {endpoint}")
        return None



def _isCertificateError(e: BaseException) -> bool:
    """True if a certificate verification failure is somewhere in the exception chain"""
    seen = set()
    stack = [e]
    while stack:
        e = stack.pop()
        if e is None or id(e) in seen:
            continue
        seen.add(id(e))
        if isinstance(e, ssl.SSLCertVerificationError):
            return True
        stack.extend([e.__cause__, e.__context__] + [a for a in getattr(e, "args", ()) if isinstance(a, BaseException)])
        reason = getattr(e, "reason", None)
        if isinstance(reason, BaseException):
            stack.append(reason)
    return False
