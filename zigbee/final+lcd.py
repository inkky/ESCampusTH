#coding:utf-8
#!/usr/bin/python

from gps import *
import time
import RPi.GPIO as GPIO
import serial
import struct
import sys

import Adafruit_DHT

######led###################
#gpio's :
SCLK = 12
DIN = 16
DC = 18
RST = 22
font =[
0x3e,0x51,0x49,0x45,0x3e,#0 
0x00,0x42,0x7f,0x40,0x00,#1 
0x42,0x61,0x51,0x49,0x46,#2 
0x21,0x41,0x45,0x4b,0x31,#3 
0x18,0x14,0x12,0x7f,0x10,#4 
0x27,0x45,0x45,0x45,0x39,#5 
0x3c,0x4a,0x49,0x49,0x30,#6 
0x01,0x71,0x09,0x05,0x03,#7 
0x36,0x49,0x49,0x49,0x36,#8 
0x06,0x49,0x49,0x29,0x1e,#9
0x00,0x24,0x00,0x00,0x00,#:
0x36,0x49,0x49,0x49,0x36,#59
0x36,0x49,0x49,0x49,0x36,#60
0x36,0x49,0x49,0x49,0x36,#61
0x36,0x49,0x49,0x49,0x36,#62
0x36,0x49,0x49,0x49,0x36,#63
0x06,0x49,0x49,0x29,0x1e,#64
0x7E, 0x11, 0x11, 0x11, 0x7E, # A
0x7F, 0x49, 0x49, 0x49, 0x36, # B
0x3E, 0x41, 0x41, 0x41, 0x22, # C
0x7F, 0x41, 0x41, 0x22, 0x1C, # D
0x7F, 0x49, 0x49, 0x49, 0x41, # E
0x7F, 0x09, 0x09, 0x09, 0x01, # F
0x3E, 0x41, 0x49, 0x49, 0x7A, # G
0x7F, 0x08, 0x08, 0x08, 0x7F, # H
0x00, 0x41, 0x7F, 0x41, 0x00, # I
0x20, 0x40, 0x41, 0x3F, 0x01, # J
0x7F, 0x08, 0x14, 0x22, 0x41, # K
0x7F, 0x40, 0x40, 0x40, 0x40, # L
0x7F, 0x02, 0x0C, 0x02, 0x7F, # M
0x7F, 0x04, 0x08, 0x10, 0x7F, # N
0x3E, 0x41, 0x41, 0x41, 0x3E, # O
0x7F, 0x09, 0x09, 0x09, 0x06, # P
0x3E, 0x41, 0x51, 0x21, 0x5E, # Q
0x7F, 0x09, 0x19, 0x29, 0x46, # R
0x46, 0x49, 0x49, 0x49, 0x31, # S
0x01, 0x01, 0x7F, 0x01, 0x01, # T
0x3F, 0x40, 0x40, 0x40, 0x3F, # U
0x1F, 0x20, 0x40, 0x20, 0x1F, # V
0x3F, 0x40, 0x38, 0x40, 0x3F, # W
0x63, 0x14, 0x08, 0x14, 0x63, # X
0x07, 0x08, 0x70, 0x08, 0x07, # Y
0x61, 0x51, 0x49, 0x45, 0x43, # Z
]

def gotoxy(x,y):
  lcd_cmd(x+128)
  lcd_cmd(y+64)
def text(words):
  for i in range(len(words)):
 #   print (words[i])
    display_char(words[i])
def display_char(char):
  index=(ord(char)-48)*5
  if ord(char) >=48 and ord(char) <=90:
    for i in range(5):
  #    print (index+i)
      lcd_data(font[index+i])
    lcd_data(0) # space inbetween characters
  elif ord(char)==32:
      lcd_data(0)
      lcd_data(0)
      lcd_data(0)
      lcd_data(0)
      lcd_data(0)
      lcd_data(0)
def cls():
  gotoxy(0,0)
  for i in range(84):
    for j in range(6):
      lcd_data(0)
def setup():
  # set pin directions
  GPIO.setmode(GPIO.BOARD)
  GPIO.setup(DIN, GPIO.OUT)
  GPIO.setup(SCLK, GPIO.OUT)
  GPIO.setup(DC, GPIO.OUT)
  GPIO.setup(RST, GPIO.OUT)
def begin(contrast):
  setup()
  # toggle RST low to reset
  GPIO.output(RST, False)
  time.sleep(0.100)
  GPIO.output(RST, True)
  lcd_cmd(0x21) # extended mode
  lcd_cmd(0x14) # bias
  lcd_cmd(contrast) # vop
  lcd_cmd(0x20) # basic mode
  lcd_cmd(0xc) # non-inverted display
  cls()
def SPI(c):
  # data = DIN
  # clock = SCLK
  # MSB first
  # value = c
  for i in xrange(8):
    GPIO.output(DIN, (c & (1 << (7-i))) > 0)
    GPIO.output(SCLK, True)
    GPIO.output(SCLK, False)
def lcd_cmd(c):
  #print ("lcd_cmd sent :",hex(c))
  GPIO.output(DC, False)
  SPI(c)
def lcd_data(c):
  #print ("data sent :",hex(c))
  GPIO.output(DC, True)
  SPI(c)

begin(0xbc) # contrast - may need tweaking for each display

######serial###################
ser=serial.Serial(
    port='/dev/ttyUSB1',  #set port
    baudrate = 38400,   #ser baud rate
    parity=serial.PARITY_NONE,
    stopbits= serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1
  )
print "Whether the serial is open:", ser.isOpen()

######gps###################
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

##DHT11
                humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 7)

                if humidity is not None and temperature is not None:
                        print('Temp={0:0.1f}*  Humidity={1:0.1f}%'.format(temperature, humidity))
##GPS
                        if report['class'] == 'TPV':
                            print 'latitude    ' , report.lat
                            print 'longitude   ' , report.lon
##数据传输
                            humi = int(humidity) #4 bytes humidity
                            temp = int(temperature)
                            start = 0xFD
                            length = 0x12
                            dest = 0x036A
                            src = 0x036C
                            longitude =report.lon
                            ew =0
                            latitude = report.lat
                            ns = 3
##lcd显示
                            longi=int(longitude)
                            latit=int(latitude)
                            gotoxy(0,0)
                            text("HUMIDITY:")
                            gotoxy(130,0)
                            text(str(humi))
                            gotoxy(0,1)
                            text("TEMPERATURE:")
                            gotoxy(130,1)
                            text(str(temp))
                            gotoxy(0,2)
                            text("LONGITUDE:")
                            gotoxy(130,2)
                            text(str(longi))
                            gotoxy(0,3)
                            text("LATITUDE:")
                            gotoxy(130,3)
                            text(str(latit))


  
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


                time.sleep(0.1)
except StopIteration:
    print "GPSD has terminated"
