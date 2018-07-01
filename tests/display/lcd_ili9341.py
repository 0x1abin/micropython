import time
from machine import SPI, Pin
from display import LCD

spi = SPI(1, baudrate=32000000, mosi=Pin(23), miso=Pin(19), sck=Pin(18))

lcd = LCD(spi = spi)

lcd.fillScreen(lcd.color565(0xFF, 0x00, 0x00))
time.sleep(1)
lcd.fillScreen(lcd.color565(0x00, 0xFF, 0x00))
time.sleep(1)
lcd.fillScreen(lcd.color565(0x00, 0x00, 0xFF))
time.sleep(1)