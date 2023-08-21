import time
import random
import errno
from picographics import PicoGraphics, DISPLAY_PICOVISION, PEN_DV_RGB555 as PEN
from machine import Pin

"""
Pimoroni logo bouncing around on the screen

Press Y to add sprites to the screen
"""

# set up the display and drawing variables
display = PicoGraphics(DISPLAY_PICOVISION, width=640, height=480, pen_type=PEN)

WIDTH, HEIGHT = display.get_bounds()
BLACK = display.create_pen(0, 0, 0)
WHITE = display.create_pen(255, 255, 255)

# image indices
LOGOSUB1_INDEX = 1
LOGOSUB2_INDEX = 2
LOGOSUB3_INDEX = 3
LOGOSUB4_INDEX = 4

SPLATSUB1_INDEX = 5
SPLATSUB2_INDEX = 6
SPLATSUB3_INDEX = 7
SPLATSUB4_INDEX = 8

MAX_SPRITE_SLOTS = 16 
MAX_LOGOS = MAX_SPRITE_SLOTS //4

y_btn = Pin(9, Pin.IN, Pin.PULL_UP)


# load the sprite file
has_sprite = False  # set if the sprite file exists in the pico


try:
    for _ in range(2):
        display.load_sprite("pim1.png", LOGOSUB1_INDEX)
        display.load_sprite("pim2.png", LOGOSUB2_INDEX)
        display.load_sprite("pim3.png", LOGOSUB3_INDEX)
        display.load_sprite("pim4.png", LOGOSUB4_INDEX)
        
        # load explosion
        display.load_sprite("fire2-0-0.png", SPLATSUB1_INDEX)
        display.load_sprite("fire2-1-0.png", SPLATSUB2_INDEX)
        display.load_sprite("fire2-0-1.png", SPLATSUB3_INDEX)
        display.load_sprite("fire2-1-1.png", SPLATSUB4_INDEX)
        display.update()
        
        

    has_sprite = True
except OSError as ioe:
    if ioe.errno not in (errno.ENOENT):
        raise
    has_sprite = False

logos = []
logos_count = 0

# specify image dimensions and randomise starting position
IMAGE_WIDTH = 64
IMAGE_HEIGHT = 64
DEFAULT_VELOCITY = 15  # set the default velocity of the image


class Logo:
    def __init__(self):
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

    def draw(self, sprite_slot):
        sprite_slot += 1
        if has_sprite:
            display.display_sprite(((sprite_slot *4) -3),LOGOSUB1_INDEX,self.x_start, self.y_start)
            display.display_sprite((sprite_slot *4) -2,LOGOSUB2_INDEX,self.x_start + 32, self.y_start)
            display.display_sprite((sprite_slot *4) -1,LOGOSUB3_INDEX,self.x_start, self.y_start + 32)
            display.display_sprite(sprite_slot*4,LOGOSUB4_INDEX,self.x_start + 32, self.y_start + 32)
        else:
            display.rectangle(self.x_start, self.y_start, 100, 100)
            display.set_pen(WHITE)
            display.text(
                "File not found",
                self.x_start,
                (self.y_start + (self.y_end - self.y_start) // 2),
                scale=1,
            )



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
    splattered = False
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
                add_splatter(logos[j].x_start, logos[j].y_start)
                splattered = True

            # left collision
            elif (
                abs(logos[j].x_start - logos[i].x_end) < x_offset
                and abs(logos[i].y_start - logos[j].y_start) <= IMAGE_HEIGHT
            ):
                if logos[i].x_vel > 0:
                    logos[i].x_vel *= -1
                if logos[j].x_vel < 0:
                    logos[j].x_vel *= -1
                add_splatter(logos[j].x_start, logos[j].y_start)
                splattered = True

            # up collision
            elif (
                abs(logos[i].y_start - logos[j].y_end) < y_offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel < 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel > 0:
                    logos[j].y_vel *= -1
                add_splatter(logos[j].x_start, logos[j].y_start)
                splattered = True

            # down collision
            elif (
                abs(logos[j].y_start - logos[i].y_end) < y_offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel > 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel < 0:
                    logos[j].y_vel *= -1
                add_splatter(logos[j].x_start, logos[j].y_start)
                splattered = True
                
    if not splattered:
        clear_splatter()


# add logos
def add_logo():
    global logos, logos_count
    new_logo = Logo()
    logos.append(new_logo)

    logos_count += 1
def add_splatter(x, y):
    display.display_sprite(17,SPLATSUB1_INDEX,x, y)
    display.display_sprite(18,SPLATSUB2_INDEX,x + 32, y)
    display.display_sprite(19,SPLATSUB3_INDEX,x, y + 32)
    display.display_sprite(20,SPLATSUB4_INDEX,x + 32, y + 32)

def clear_splatter():
    x = 800
    y = 600
    display.display_sprite(17,SPLATSUB1_INDEX,x, y)
    display.display_sprite(18,SPLATSUB2_INDEX,x + 32, y)
    display.display_sprite(19,SPLATSUB3_INDEX,x, y + 32)
    display.display_sprite(20,SPLATSUB4_INDEX,x + 32, y + 32)

def hsv_to_rgb(h, s, v):
    if s == 0.0:
        return v, v, v
    i = int(h * 6.0)
    f = (h * 6.0) - i
    p = v * (1.0 - s)
    q = v * (1.0 - s * f)
    t = v * (1.0 - s * (1.0 - f))
    i = i % 6
    if i == 0:
        return v, t, p
    if i == 1:
        return q, v, p
    if i == 2:
        return p, v, t
    if i == 3:
        return p, q, v
    if i == 4:
        return t, p, v
    if i == 5:
        return v, p, q


hue = 0
def draw_background():
    global hue
    hue += 1
    r, g, b = [int(255 * c) for c in hsv_to_rgb(hue / 360.0, 1.0, 1.0)]  # rainbow magic
    RAINBOW = display.create_pen(r, g, b)  # Create pen with converted HSV value
    display.set_pen(RAINBOW)  # Set pen
    display.clear()

    
    
text_colour = WHITE

add_logo()  # add initial logo


last_time = time.ticks_ms()

while True:
    current_time = time.ticks_ms()

    # add logo when Y button is pressed
    if current_time - last_time > 500:
        if not y_btn.value() and logos_count < MAX_LOGOS:
            add_logo()
            print(logos_count)
        last_time = current_time

    # fills the screen with black
    draw_background()
        
    
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
        logo.draw(num)

    display.update()
