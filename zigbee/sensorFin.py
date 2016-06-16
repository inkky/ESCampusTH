#coding:utf-8
#!/usr/bin/python

from gps import *
import time
import RPi.GPIO as GPIO
import serial
import struct
import sys

import Adafruit_DHT


ser=serial.Serial(
    port='/dev/ttyUSB0',  #set port
    baudrate = 38400,   #ser baud rate
    parity=serial.PARITY_NONE,
    stopbits= serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1
  )

print "Whether the serial is open:", ser.isOpen()

session = gps(mode=WATCH_ENABLE)


#report = session.next()

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
                
                humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 7)

                if humidity is not None and temperature is not None:
                        print('Temp={0:0.1f}*  Humidity={1:0.1f}%'.format(temperature, humidity))

                        if report['class'] == 'TPV':
                            print 'latitude    ' , report.lat
                            print 'longitude   ' , report.lon

                            humi = int(humidity) #4 bytes humidity
                            temp = int(temperature)
                            start = 0xFD
                            length = 0x12
                            dest = 0x036A
                            src = 0x036C
                            longitude = report.lon
                            ew = 1
                            latitude = report.lat
                            ns = 3

                            data = struct.pack("<BB2HfBfBII",
                              start,
                              length,
                              dest,
                              src,
                              longitude,
                              ew,
                              latitude,
                              ns,
                              temp,
                              humi                             
                             )
                            ser.write(data)
                else:
                        print('Failed to get reading. Try again!')
          
                
                time.sleep(1)
except StopIteration:
    print "GPSD has terminated"