"""
Simple demo of how to read the buttons on PicoVision. This example doesn't use the display.

Buttons A and X are wired to the GPU, whilst Y is wired to the CPU, so you need to read them a little differently.
"""

from picovision import PicoVision, PEN_RGB555
import machine
import time

display = PicoVision(PEN_RGB555, 640, 480)

button_y = machine.Pin(9, machine.Pin.IN, machine.Pin.PULL_UP)

print("Press a button!")

while True:
    if button_y.value() == 0:
        print("Y is pressed")
        time.sleep(0.5)
    if display.is_button_x_pressed():
        print("X is pressed")
        time.sleep(0.5)
    if display.is_button_a_pressed():
        print("A is pressed")
        time.sleep(0.5)
