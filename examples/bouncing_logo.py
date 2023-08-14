import time
import random
import errno
import math
from picographics import PicoGraphics, DISPLAY_PICOVISION, PEN_DV_RGB555 as PEN
from pngdec import PNG
from machine import Pin


"""
Pimoroni logo bouncing around on the screen
Ensure sprite file in the pico
Press Y to add sprites to the screen
"""

# set up the display and drawing variables
display = PicoGraphics(DISPLAY_PICOVISION, width=640, height=480, pen_type=PEN)

WIDTH, HEIGHT = display.get_bounds()
BLACK = display.create_pen(0, 0, 0)
WHITE = display.create_pen(255, 255, 255)

y_btn = Pin(9, Pin.IN, Pin.PULL_UP)

# load the sprite file
has_sprite = False  # set if the sprite file exists in the pico
png = PNG(display)

try:
    png.open_file("pimpic.png")
    has_sprite = True
except OSError as ioe:
    if ioe.errno not in (errno.ENOENT,):
        raise
    has_sprite = False

logos = []
logos_count = 0

# specify image dimensions and randomise starting position
IMAGE_WIDTH = 100
IMAGE_HEIGHT = 100
DEFAULT_VELOCITY = 30  # set the default velocity of the image


class Logo:
    def __init__(self, png):
        self.png = png
        self.x_start = random.randint(50, WIDTH - IMAGE_WIDTH)
        self.y_start = random.randint(50, HEIGHT - IMAGE_HEIGHT)
        self.x_end = self.x_start + IMAGE_WIDTH
        self.y_end = self.y_start + IMAGE_HEIGHT
        self.x_vel = (
            DEFAULT_VELOCITY if random.randint(0, 1) == 1 else -DEFAULT_VELOCITY
        )
        self.y_vel = (
            DEFAULT_VELOCITY if random.randint(0, 1) == 1 else -DEFAULT_VELOCITY
        )

    def draw(self):
        if has_sprite:
            png.decode(self.x_start, self.y_start, scale=(2, 2))
        else:
            display.rectangle(self.x_start, self.y_start, 100, 100)


# generate random colour when called
def change_pen_colour():
    r = random.randint(20, 255)
    g = random.randint(20, 255)
    b = random.randint(20, 255)
    text_colour = display.create_pen(r, g, b)
    return text_colour


# check for edge collisions and adjust velocities and positions accordingly
def edge_collision(logo):
    global text_colour
    if (logo.x_start + IMAGE_WIDTH) >= WIDTH or logo.x_start < 0:
        logo.x_vel = -logo.x_vel
        if logo.x_start < 0:
            logo.x_start = 0
        elif (logo.x_start + IMAGE_WIDTH) > WIDTH:
            logo.x_start = WIDTH - IMAGE_WIDTH
        text_colour = change_pen_colour()

    if (logo.y_start + IMAGE_HEIGHT) >= HEIGHT or logo.y_start <= 0:
        logo.y_vel = -logo.y_vel
        if logo.y_start < 0:
            logo.y_start = 0
        elif (logo.y_start + IMAGE_HEIGHT) > HEIGHT:
            logo.y_start = HEIGHT - IMAGE_HEIGHT
        text_colour = change_pen_colour()


# handle logo collisions
x_offset = DEFAULT_VELOCITY // 2
y_offset = DEFAULT_VELOCITY // 2

def object_collision(j):
    for i in range(logos_count):
        if j != i:  # ensures distinct logos
             # right collision
            if (
                abs(logos[i].x_start - logos[j].x_end) < x_offset
                and abs(logos[i].y_start - logos[j].y_start) <= IMAGE_HEIGHT
            ):
                if logos[i].x_vel < 0:
                    logos[i].x_vel *= -1
                if logos[j].x_vel > 0:
                    logos[j].x_vel *= -1

            # left collision
            elif (
                abs(logos[j].x_start - logos[i].x_end) < x_offset
                and abs(logos[i].y_start - logos[j].y_start) <= IMAGE_HEIGHT
            ):
                if logos[i].x_vel > 0:
                    logos[i].x_vel *= -1
                if logos[j].x_vel < 0:
                    logos[j].x_vel *= -1

            # up collision
            elif (
                abs(logos[i].y_start - logos[j].y_end) < y_offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel < 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel > 0:
                    logos[j].y_vel *= -1

            # down collision
            elif (
                abs(logos[j].y_start - logos[i].y_end) < y_offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel > 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel < 0:
                    logos[j].y_vel *= -1


# add logos
def add_logo():
    global logos, logos_x, logos_y, logos_count
    new_logo = Logo(png)
    logos.append(new_logo)
    logos_count += 1


text_colour = WHITE

add_logo()  # add initial logo


last_time = time.ticks_ms()


while True:
    current_time = time.ticks_ms()

    # add logo when Y button is pressed
    if current_time - last_time > 500:
        if y_btn.value() == False:
            add_logo()
        last_time = current_time

    # fills the screen with black
    display.set_pen(BLACK)
    display.clear()

    display.set_pen(text_colour)
    text_width = display.measure_text("PIMORONI", scale=10)
    display.text("PIMORONI", ((WIDTH - text_width) // 2), HEIGHT // 2, scale=10)

    # update positions for each logo and account for collisions
    for num in range(logos_count):
        logo = logos[num]
        logo.x_start += logo.x_vel
        logo.y_start += logo.y_vel
        logo.x_end = logo.x_start + IMAGE_WIDTH
        logo.y_end = logo.y_start + IMAGE_HEIGHT
        edge_collision(logo)
        object_collision(num)
        logo.draw()

    display.update()
