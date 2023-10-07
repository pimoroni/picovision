"""
Bitmap font demo!

Bitmap fonts are fast but blocky. They are best used for small text.
"""

from picovision import PicoVision, PEN_RGB555

display = PicoVision(PEN_RGB555, 640, 480)

# variable to keep track of pen hue
hue = 0.0

display.set_font("bitmap8")

for i in range(14):
    # create a pen and set the drawing color
    PEN_COLOUR = display.create_pen_hsv(hue, 1.0, 1.0)
    display.set_pen(PEN_COLOUR)
    # draw text
    display.text("Hello PicoVision!", i * 30, i * 40, scale=4)
    # increment hue
    hue += 1.0 / 14

display.update()
