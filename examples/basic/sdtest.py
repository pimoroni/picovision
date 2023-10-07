# This simple example shows how to read and write from the SD card on PicoVision.
# You will need sdcard.py saved to your Pico.
# Find it in https://github.com/pimoroni/pimoroni-pico/tree/main/micropython/examples/common
from machine import Pin, SPI
import sdcard
import os

# set up the SD card
sd_spi = SPI(1, sck=Pin(10, Pin.OUT), mosi=Pin(11, Pin.OUT), miso=Pin(12, Pin.OUT))
sd = sdcard.SDCard(sd_spi, Pin(15))
os.mount(sd, "/sd")

# create a file and add some text
# if this file already exists it will be overwritten
with open("/sd/sdtest.txt", "w") as f:
    f.write("Hello PicoVision!\r\n")
    f.close()

# open the file and append some text (useful for logging!)
with open("/sd/sdtest.txt", "a") as f:
    f.write("We're appending some text\r\n")
    f.close()

# read the file and print the contents to the console
with open("/sd/sdtest.txt", "r") as f:
    data = f.read()
    print(data)
    f.close()

# unmount the SD card
os.umount("/sd")
