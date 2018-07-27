import time
from machine import SPI, Pin
from display import LCD

spi = SPI(1, baudrate=32000000, mosi=Pin(23), miso=Pin(19), sck=Pin(18))

lcd = LCD(spi = spi)
lcd.fillScreen(lcd.color565(0x00, 0x00, 0x00))
lcd.line(0,0,44,44,lcd.ORANGE)
lcd.triangle(22,22,69,98,51,22,lcd.RED)
lcd.circle(100,100,10,lcd.BLUE)
lcd.print('LLLLLLLL',80,80)
