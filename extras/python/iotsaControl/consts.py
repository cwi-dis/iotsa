from __future__ import print_function
from __future__ import unicode_literals
from builtins import object

VERBOSE=False

class UserIntervention(Exception):
    pass

class IotsaError(RuntimeError):
    pass
    
class CoapError(Exception):
    pass
