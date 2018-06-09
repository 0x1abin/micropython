'''
 __  __ ____ ____  _             _    
|  \/  | ___/ ___|| |_ __ _  ___| | __
| |\/| |___ \___ \| __/ _` |/ __| |/ /
| |  | |___) |__) | || (_| | (__|   < 
|_|  |_|____/____/ \__\__,_|\___|_|\_\
'''

import os
import time
import network
import machine
import _thread

sta_if = network.WLAN(network.STA_IF); sta_if.active(True)
# sta_if.scan()                             # Scan for available access points
sta_if.connect("MasterHax_5G", "wittyercheese551") # Connect to an AP

uart = machine.UART(2)
uart.init(115200)

while not sta_if.isconnected():                      # Check for successful connection
  print('.', end='')
  time.sleep(0.2)
print('Connected!')


def func(param=666):
  print("func start")
  while True:
    print(param)
    time.sleep(1)
    _thread.exit()
