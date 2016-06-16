#!/usr/bin/python
import sys
import Adafruit_DHT

# Parse command line parameters.
#DHT11
humidity, temperature = Adafruit_DHT.read_retry(Adafruit_DHT.DHT11, 7)

if humidity is not None and temperature is not None:
    print('Temp={0:0.1f}*  Humidity={1:0.1f}%'.format(temperature, humidity))
else:
    print('Failed to get reading. Try again!')
    sys.exit(1)
