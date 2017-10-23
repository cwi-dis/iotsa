"""Simple program to do timed reads from all configured iotsaSimpleIO input
ports and generate a CSV file.
"""
import argparse
import sys
import time
import urllib
import json
import csv

class SimpleIOReader:
    def __init__(self, url, interval=1, count=None, unixtime=False, reportErrors=False, ignoreErrors=False):
        self.url = url
        self.interval = interval
        self.count = count
        self.unixtime = unixtime
        self.reportErrors = reportErrors
        self.ignoreErrors = ignoreErrors
        
        self.csvOutput = None
        self.lastProcessTime = None
        
    def run(self):
        while True:
            self.lastProcessTime = time.time()
            ok = self.processOne()
            if not ok and not self.reportErrors and not self.ignoreErrors:
                return False
            if self.count != None:
                self.count -= 1
                if self.count <= 0:
                    return True
            nextProcessTime = self.lastProcessTime + self.interval
            delta = nextProcessTime - time.time()
            if delta > 0:
                time.sleep(delta)
            
    def processOne(self):
        try:
            fp = urllib.urlopen(self.url)
        except IOError as e:
            return self.reportError(str(e))
        try:
            jsonData = fp.read()
        except IOError as e:
            return self.reportError(str(e))
        if fp.code != 200:
            return self.reportError(jsonData)
        try:
            data = json.loads(jsonData)
        except ValueError as e:
            return self.reportError(str(e))
        if type(data) != type({}):
            return self.reportError("JSON data must be an object")
        if self.unixtime:
            data['unixtime'] = time.time()
        self.reportData(data)
        return True
        
    def reportError(self, error):
        if self.reportError and self.csvOutput:
            data = {'error' : error}
            if self.unixtime:
                data['unixtime'] = time.time()
            self.reportData(data)
            return True
        print >> sys.stderr, '%s: %s' % (sys.argv[0], error)
        if self.reportError or self.ignoreError:
            return True
        return False
        
    def reportData(self, data):
        if not self.csvOutput:
            # First record. Create writer.
            keylist = data.keys()
            if 'unixtime' in keylist: keylist.remove('unixtime')
            if 'error' in keylist: keylist.remove('error')
            keylist.sort()
            if 'timestamp' in keylist:
                keylist.remove('timestamp')
                keylist.insert(0, 'timestamp')
            if self.unixtime:
                keylist.insert(0, 'unixtime')
            if self.reportErrors:
                keylist.append('error')
            self.csvOutput = csv.DictWriter(sys.stdout, fieldnames=keylist, quoting=csv.QUOTE_NONNUMERIC)
            self.csvOutput.writeheader()
        self.csvOutput.writerow(data)
        sys.stdout.flush()
                
def main():
    parser = argparse.ArgumentParser(description="Produce CSV file from iotsaSimpleIO readings")
    parser.add_argument('-H', '--host', dest='host', action='store', default=None, metavar='hostname', help='iotsaSimpleIO host')
    parser.add_argument('--url', dest='url', action='store', default=None, metavar='URL', help='iotsaSimpleIO URL (default: based on --host)')
    parser.add_argument('-i', '--interval', dest='interval', action='store', type=float, default=1, help='Interval in seconds between reads (default: 1s)')
    parser.add_argument('-c', '--count', dest='count', action='store', type=int, help='Stop after reading this many records')
    parser.add_argument('-u', '--unixtime', dest='unixtime', action='store_true', help='Store unix reception timestamp in each record')
    parser.add_argument('-e', '--reportErrors', dest='reportErrors', action='store_true', help='Report errors (in a CSV column) and continue')
    parser.add_argument('-E', '--ignoreErrors', dest='ignoreErrors', action='store_true', help='Ignore errors and continue (default: stop)')
    args = parser.parse_args(sys.argv[1:])
    if args.url:
        url = args.url
    elif args.host:
        url = 'http://%s/api' % args.host
    else:
        print >>sys.stderr, "%s: one of --host or --url must be specified" % sys.argv[0]
        sys.exit(2)
    r = SimpleIOReader(url, args.interval, args.count, args.unixtime, args.reportErrors, args.ignoreErrors)
    ok = r.run()
    if not ok:
        sys.exit(1)
        
if __name__ == '__main__':
    main()
    
