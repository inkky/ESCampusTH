#coding:utf-8
from gps import *
import time
session = gps(mode=WATCH_ENABLE)
try:
        while True:
                report = session.next()
                #print report
                if report['class'] == 'VERSION':
                        print 'connect GPS suc.'
                if report['class'] == 'DEVICES':
                        print 'searching satellite ing....'
                if report['class'] == 'WATCH':
                        print 'search satellite suc.'
                if report['class'] == 'TPV':
                        print '----------GPS------------'
                        print 'time     ' , report.time
                        print 'latitude    ' , report.lat, 'E'
                        print 'longitude   ' , report.lon, 'N'
                        print '----------END-------------'
                time.sleep(3)
except StopIteration:
    print "GPSD has terminated"