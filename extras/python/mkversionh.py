#!/usr/bin/env python
import json
import sys
import os
import re
import subprocess

# Import("env")
# print("mkversionh: env: ", env.Dump(), file=sys.stderr)

VERBOSE=False

DEFINE_PAT = re.compile(r"^#define\s+(?P<name>\w+)\s+(?P<value>.*)\s+$")


class VersionFile:
    def __init__(self, includeFile):
        self.includeFile = includeFile
        self.defines = {}
        self.changed = False
        if VERBOSE: print(f"mkversionh: loading {self.includeFile}", file=sys.stderr)
        for line in open(self.includeFile):
            match = DEFINE_PAT.match(line)
            if match:
                self.define(match.group("name"), match.group("value"))

    def define(self, name, value):
        if self.defines.get(name, None) == value:
            if VERBOSE: print(f"mkversionh: already set: {name}={value}", file=sys.stderr)
            return
        if VERBOSE: print(f"mkversionh: {name}={value}", file=sys.stderr)
        self.defines[name] = value
        self.changed = True

    def save(self):
        fp = open(self.includeFile, "w")
        for name, value in list(self.defines.items()):
            fp.write("#define %s %s\n" % (name, value))
        if VERBOSE: print(f"mkversionh: saved {self.includeFile}", file=sys.stderr)

    def get(self, key, default=None):
        return self.defines.get(key, default)


def main():
    try:
        baseDir = os.path.dirname(__file__)
    except NameError:
        baseDir = os.getcwd()
    while not os.path.exists(os.path.join(baseDir, "library.json")):
        newBaseDir = os.path.dirname(baseDir)
        if newBaseDir == baseDir:
            break
        baseDir = newBaseDir
    libraryConfig = os.path.join(baseDir, "library.json")
    version = os.path.join(baseDir, "src", "iotsaVersion.h")
    versionpy = os.path.join(baseDir, "extras", "python", "iotsa", "version.py")
    if not os.path.exists(libraryConfig) or not os.path.exists(version):
        print(f"mkversionh: Cannot find config files {libraryConfig} and {version}. Must be run from iotsa source tree.", file=sys.stderr)
        sys.exit(1)

    vf = VersionFile(version)

    libraryData = json.load(open(libraryConfig))
    if "version" in libraryData:
        vf.define("IOTSA_VERSION", '"' + libraryData["version"] + '"')

    if 'IOTSA_FULL_VERSION' in os.environ:
        fullVersion = os.environ['IOTSA_FULL_VERSION']
    else:
        cmd = subprocess.Popen(
            "git describe --match 'v*'",
            shell=True,
            cwd=baseDir,
            stdout=subprocess.PIPE,
            universal_newlines=True,
        )
        fullVersion = cmd.stdout.read().strip()
        fullVersion = fullVersion.replace("-", "+", 1)
    shortVersion = vf.get("IOTSA_VERSION", '"unknown""')
    if not fullVersion:
        fullVersion = shortVersion
    if not '"' in fullVersion:
        fullVersion = '"' + fullVersion + '"'
    vf.define("IOTSA_FULL_VERSION", fullVersion)
    if vf.changed:
        vf.save()
    # Generate Python version file too
    PyNewVersion = f'__version__ = {fullVersion}\n__shortversion__ = {shortVersion}\n'
    PyOldVersion = open(versionpy).read()
    if PyNewVersion != PyOldVersion:
        open(versionpy, "w").write(PyNewVersion)

main()
