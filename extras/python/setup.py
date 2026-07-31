"""Regenerate iotsa/version.py from the current git state, then hand off to
setuptools. Static metadata lives in pyproject.toml; this file exists only
because the version needs to be computed at build/install time, not just
declared."""

import os
import subprocess
import sys

from setuptools import setup

here = os.path.abspath(os.path.dirname(__file__))

# Regenerate iotsa/version.py from the current git state (same logic PlatformIO
# builds use) so the version reported by `iotsa --version` reflects the commit
# this was installed from, not whatever was last committed to version.py.
try:
    subprocess.run(
        [sys.executable, os.path.join(here, "mkversionh.py")], check=True, cwd=here
    )
except Exception as e:
    print(
        f"setup.py: warning: could not regenerate iotsa/version.py ({e}), using existing value",
        file=sys.stderr,
    )

setup()
