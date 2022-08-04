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
    baseDir = os.getcwd()
    baseDir = os.path.dirname(baseDir)
    libraryConfig = os.path.join(baseDir, "library.json")
    version = os.path.join(baseDir, "src", "iotsaVersion.h")
    if not os.path.exists(libraryConfig) or not os.path.exists(version):
        print(f"mkversionh: Cannot find config files {libraryConfig} and {version}. Must be run from iotsa source tree.", file=sys.stderr)
        sys.exit(1)

    vf = VersionFile(version)

    libraryData = json.load(open(libraryConfig))
    if "version" in libraryData:
        vf.define("IOTSA_VERSION", '"' + libraryData["version"] + '"')

    cmd = subprocess.Popen(
        "git rev-parse --short HEAD",
        shell=True,
        cwd=baseDir,
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    commit = cmd.stdout.read().strip()
    if commit:
        vf.define("IOTSA_COMMIT", '"' + commit + '"')
    else:
        if VERBOSE: print("mkversionh: cannot git rev-parse to get version information", file=sys.stderr)
    fullVersion = vf.get("IOTSA_VERSION", '"unknown""')
    commit = vf.get("IOTSA_COMMIT")
    if commit:
        fullVersion = '"' + eval(fullVersion) + '+sha' + eval(commit) + '"'
    vf.define("IOTSA_FULL_VERSION", fullVersion)
    if vf.changed:
        vf.save()

main()
