"""
Simple demo of how to display text on PicoVision.
"""

from picovision import PicoVision, PEN_RGB555

display = PicoVision(PEN_RGB555, 640, 480)

WHITE = display.create_pen(255, 255, 255)

display.set_pen(WHITE)
display.text("Hello PicoVision!", 0, 0, scale=4)
display.update()
