import time
from machine import SPI, Pin
from display import LCD

spi = SPI(1, baudrate=32000000, mosi=Pin(23), miso=Pin(19), sck=Pin(18))

lcd = LCD(spi = spi)
lcd.fillScreen(lcd.BLACK)

lcd.line(0,0,44,44,lcd.ORANGE)
lcd.drawTriangle(22,22,69,98,51,22,lcd.RED)
lcd.fillcircle(100,100,10,lcd.BLUE)
lcd.print('LLLLLLLL',80,80)
lcd.clear()
lcd.setRotation(2)
print(lcd.get_fg())
print(lcd.get_bg())
lcd.set_fg(lcd.BLACK)
lcd.set_bg(lcd.WHITE)
lcd.setTextColor(lcd.ORANGE)
lcd.setTextColor(lcd.RED, lcd.BLUE)



