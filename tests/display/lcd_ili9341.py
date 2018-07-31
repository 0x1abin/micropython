import time
from machine import SPI, Pin
from display import LCD

spi = SPI(1, baudrate=32000000, mosi=Pin(23), miso=Pin(19), sck=Pin(18))

lcd = LCD(spi = spi)
lcd.fillScreen(lcd.BLACK)

# lcd.drawLine(0,0,44,44,lcd.ORANGE)
# lcd.drawTriangle(22,22,69,98,51,22,lcd.RED)
# lcd.fillcircle(100,100,10,lcd.BLUE)
# lcd.print('LLLLLLLL',80,80)
# lcd.clear()
# lcd.setTextColor(lcd.ORANGE)
# lcd.setTextColor(lcd.RED, lcd.BLUE)
# print('Current cursor is (%d, %d)' % (lcd.getCursor))
# lcd.setCursor(100, 200)
# print('New cursor is (%d, %d)' % (lcd.getCursor))

lcd.setRotation(2)
lcd.setColor(lcd.RED)   
lcd.setColor(lcd.ORANGE, LCD.DARKCYAN)   
lcd.setTextColor(lcd.PINK)  
lcd.setTextColor(lcd.ORANGE, LCD.DARKCYAN)  
lcd.drawPixel(22,22,lcd.RED)	  
lcd.fillScreen(lcd.YELLOW)	  
lcd.drawLine(0,0,lcd.WHITE)	  
lcd.drawTriangle(22,22,69,98,51,22,lcd.RED)
lcd.fillTriangle(122,122,169,198,151,182,lcd.RED)
lcd.drawCircle(180,180,10,lcd.BLUE)	  
lcd.fillcircle(100,100,10,lcd.BLUE)	  
lcd.drawRect(180,12,122,10,lcd.BLUE)
lcd.fillRect(180,30,122,10,lcd.BLUE)	  
lcd.drawRoundRect(180,50,122,10,4,lcd.BLUE)	  
lcd.fillRoundRect(180,70,122,10,4,lcd.BLUE)	  
lcd.print('this is a print text function', 80, 80)	      
lcd.clear()		



