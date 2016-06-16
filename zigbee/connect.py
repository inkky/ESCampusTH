#!usr/bin/env python
import time
import serial
import struct

ser=serial.Serial(
    port='/dev/ttyUSB1',	#set port
    baudrate = 38400,		#ser baud rate
	parity=serial.PARITY_NONE,
	stopbits= serial.STOPBITS_ONE,
	bytesize=serial.EIGHTBITS,
	timeout=1
	)

print "Whether the serial is open:", ser.isOpen()

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
data = struct.pack("<BB2HfBfBII",start,length,dest,src,longitude,ew,latitude,ns,temp,humi)

while 1:
	ser.write(data)
	time.sleep(10)