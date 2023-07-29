import time
from typing import Optional, Tuple
import requests

from ..consts import VERBOSE
from .abstract import IotsaAbstractProtocolHandler

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

    def close(self):
        pass

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
                r = requests.request(
                    method,
                    url,
                    auth=self.auth,
                    json=json,
                    verify=not self.noverify,
                    headers=headers,
                    files=files,
                )
            except requests.exceptions.SSLError:
                raise
            except requests.exceptions.ConnectionError:
                if retryCount > 0:
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
        r.raise_for_status()
        if r.text and r.text[0] == "{":
            return r.json()
        return None

