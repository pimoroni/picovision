"""
Vector font demo! Vector fonts are slower but smoother. They are best used for large text.

You will need to copy the .af font files to your PicoVision
(this script assumes they are in the basic/ directory).

Spicy Soup font originally from https://www.dafont.com/spicy-soup.font
Next Sunday font originally from https://www.dafont.com/next-sunday.font
Coolvetica font originally from https://www.dafont.com/coolvetica.font

Find out how to convert your own fonts to .af here: https://github.com/lowfatcode/alright-fonts
"""

from picovision import PicoVision, PEN_RGB555
from picovector import PicoVector, ANTIALIAS_X16
import time

display = PicoVision(PEN_RGB555, 640, 480)
vector = PicoVector(display)

WIDTH, HEIGHT = display.get_bounds()
BLACK = display.create_pen(0, 0, 0)

# variable to keep track of hue
hue = 0.0

# antialiasing draws the vector multiple times for a smoother look
vector.set_antialiasing(ANTIALIAS_X16)

while True:
    # reset the hue
    hue = 0.0

    # clear to black
    display.set_pen(BLACK)
    display.clear()
    # set the vector font and size
    vector.set_font("basic/spicy_soup.af", 72)
    # draw the text seven times
    for i in range(7):
        # create a HSV (hue, value, saturation) pen and set the drawing color
        PEN_COLOUR = display.create_pen_hsv(hue, 1.0, 1.0)
        display.set_pen(PEN_COLOUR)
        # draw text
        vector.text("Hello PicoVision!", 10, i * 65)
        # increment hue
        hue += 1.0 / 7
    display.update()
    time.sleep(5)

    display.set_pen(BLACK)
    display.clear()
    vector.set_font("basic/next_sunday.af", 55)
    # draw the text ten times, sideways
    for i in range(10):
        PEN_COLOUR = display.create_pen_hsv(hue, 1.0, 1.0)
        display.set_pen(PEN_COLOUR)
        vector.text("Hello PicoVision!", i * WIDTH // 10, HEIGHT, 270)
        hue += 1.0 / 10
    display.update()
    time.sleep(5)

    display.set_pen(BLACK)
    display.clear()
    vector.set_font("basic/coolvetica_rg.af", 60)
    # draw the text many times
    for i in range(30):
        PEN_COLOUR = display.create_pen_hsv(hue, 1.0, 1.0)
        display.set_pen(PEN_COLOUR)
        vector.text("Hello PicoVision!", WIDTH // 2, HEIGHT // 2, i * 12)
        hue += 1.0 / 30
    display.update()
    time.sleep(5)
