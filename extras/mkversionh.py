#!/usr/bin/env python
import json
import sys
import os
import re
import subprocess

DEFINE_PAT=re.compile(r'^#define\s+(?P<name>\w+)\s+(?P<value>.*)\s+$')

class VersionFile:
    def __init__(self, includeFile):
        self.includeFile = includeFile
        self.defines = {}
        self.changed = False
        for line in open(self.includeFile):
            match = DEFINE_PAT.match(line)
            if match:
                self.defines[match.group('name')] = match.group('value')
                
    def define(self, name, value):
        if self.defines.get(name, None) == value:
            return
        self.defines[name] = value
        self.changed = True
        
    def save(self):
        fp = open(self.includeFile, 'w')
        for name, value in self.defines.items():
            fp.write('#define %s %s\n' % (name, value))
            
    def get(self, key, default=None):
        return self.defines.get(key, default)
            
def main():
    progName = os.path.join(os.getcwd(), sys.argv[0])
    baseDir = os.path.dirname(os.path.dirname(progName))
    libraryConfig = os.path.join(baseDir, 'library.json')
    version = os.path.join(baseDir, 'src', 'iotsaVersion.h')
    if not os.path.exists(libraryConfig) or not os.path.exists(version):
        print '%s: Cannot find config files %s and %s. Must be run from iotsa source tree.'% (sys.argv[0], libraryConfig, version)
        sys.exit(1)
        
    vf = VersionFile(version)
    
    libraryData = json.load(open(libraryConfig))
    if 'version' in libraryData:
        vf.define('IOTSA_VERSION', '"'+libraryData['version']+'"')
        
    cmd = subprocess.Popen('git rev-parse --short HEAD', shell=True, cwd=baseDir, stdout=subprocess.PIPE)
    commit = cmd.stdout.read().strip()
    if commit:
        vf.define('IOTSA_COMMIT', '"'+commit+'"')
    fullVersion = vf.get('IOTSA_VERSION', '"unknown""')
    commit = vf.get('IOTSA_COMMIT')
    if commit:
        fullVersion = "(" + fullVersion + '"@"' + commit + ")"
    vf.define("IOTSA_FULL_VERSION", fullVersion)
    print 'xxx', fullVersion
    if vf.changed:
        vf.save()
        
if __name__ == '__main__':
    main()
    
