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
NAME = "PIMORONI"
DROP_SHADOW_OFFSET = 5
TEXT_SCALE = 10

# image indices
LOGOSUB1_INDEX = 1
LOGOSUB2_INDEX = 2
LOGOSUB3_INDEX = 3
LOGOSUB4_INDEX = 4
SUB_IMAGE_SIZE = 32

MAX_SPRITE_SLOTS = 16 
MAX_LOGOS = MAX_SPRITE_SLOTS //4

y_btn = Pin(9, Pin.IN, Pin.PULL_UP)


has_sprite = False  # set if the sprite file exists in the pico

try:
    # load the sprites
    for _ in range(2):
        display.load_sprite("pim1.png", LOGOSUB1_INDEX)
        display.load_sprite("pim2.png", LOGOSUB2_INDEX)
        display.load_sprite("pim3.png", LOGOSUB3_INDEX)
        display.load_sprite("pim4.png", LOGOSUB4_INDEX)
    
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
DEFAULT_VELOCITY = 20  # set the default velocity of the image


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

    def draw(self, logo_id):
        """ Draws the logo at its x and y positions
            In order to display logo in 64 by 64 pixels, 4 sub images are displayed
            Each subimage uses up a sprite slot
            If any sub logo image is not present in the pico a rectangle is displayed instead"""
        logo_num = logo_id + 1
        if has_sprite:
            display.display_sprite(((logo_num *4) -3), LOGOSUB1_INDEX, self.x_start, self.y_start)
            display.display_sprite(((logo_num *4) -2), LOGOSUB2_INDEX, self.x_start + SUB_IMAGE_SIZE, self.y_start)
            display.display_sprite(((logo_num *4) -1), LOGOSUB3_INDEX, self.x_start, self.y_start + SUB_IMAGE_SIZE)
            display.display_sprite((logo_num *4), LOGOSUB4_INDEX, self.x_start + SUB_IMAGE_SIZE, self.y_start + SUB_IMAGE_SIZE)
        else:
            display.rectangle(self.x_start, self.y_start, 100, 100)
            display.set_pen(WHITE)
            display.text(
                "File not found",
                self.x_start,
                (self.y_start + (self.y_end - self.y_start) // 2),
                scale=1,
            )



def edge_collision(logo):
    """Checks if logo hits the walls and reverses its velocity"""
    #  Horizontal walls
    if (logo.x_start + IMAGE_WIDTH) >= WIDTH or logo.x_start < 0:
        logo.x_vel = -logo.x_vel
        if logo.x_start < 0:
            logo.x_start = 0
        elif (logo.x_start + IMAGE_WIDTH) > WIDTH:
            logo.x_start = WIDTH - IMAGE_WIDTH
        
    #   Vertical walls
    if (logo.y_start + IMAGE_HEIGHT) >= HEIGHT or logo.y_start <= 0:
        logo.y_vel = -logo.y_vel
        if logo.y_start < 0:
            logo.y_start = 0
        elif (logo.y_start + IMAGE_HEIGHT) > HEIGHT:
            logo.y_start = HEIGHT - IMAGE_HEIGHT
        



offset = DEFAULT_VELOCITY // 2
def object_collision(j):
    """Checks if a given logo collides with any other logo on the screen"""
    for i in range(logos_count):
        if j != i:  # ensures distinct logos
            # right collision
            if (
                abs(logos[i].x_start - logos[j].x_end) < offset
                and abs(logos[i].y_start - logos[j].y_start) <= IMAGE_HEIGHT
            ):
                if logos[i].x_vel < 0:
                    logos[i].x_vel *= -1
                if logos[j].x_vel > 0:
                    logos[j].x_vel *= -1
                

            # left collision
            elif (
                abs(logos[j].x_start - logos[i].x_end) < offset
                and abs(logos[i].y_start - logos[j].y_start) <= IMAGE_HEIGHT
            ):
                if logos[i].x_vel > 0:
                    logos[i].x_vel *= -1
                if logos[j].x_vel < 0:
                    logos[j].x_vel *= -1
                

            # up collision
            elif (
                abs(logos[i].y_start - logos[j].y_end) < offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel < 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel > 0:
                    logos[j].y_vel *= -1
                

            # down collision
            elif (
                abs(logos[j].y_start - logos[i].y_end) < offset
                and abs(logos[i].x_start - logos[j].x_start) <= IMAGE_WIDTH
            ):
                if logos[i].y_vel > 0:
                    logos[i].y_vel *= -1
                if logos[j].y_vel < 0:
                    logos[j].y_vel *= -1
                
                
def add_logo():
    global logos, logos_count
    new_logo = Logo()
    logos.append(new_logo)

    logos_count += 1

def draw_background():
    """Draws a gradient background """
    t = time.ticks_ms() / 1000.0
    grid_size = 80
    for y in range(0, HEIGHT// grid_size):
        for x in range(0, WIDTH // grid_size):
            h = x + y + int(t * 5)
            h = h / 50.0
            

            display.set_pen(display.create_pen_hsv(h, 0.5, 1))
            display.rectangle(x * grid_size, y * grid_size, grid_size, grid_size)


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

    
    draw_background()
    
    # display name with offset
    text_width = display.measure_text(NAME, scale=TEXT_SCALE)
    text_height = int(7.5 * TEXT_SCALE)
    display.set_pen(BLACK)
    display.text(NAME, ((WIDTH - text_width) // 2) - DROP_SHADOW_OFFSET, (HEIGHT // 2 - text_height//2) + DROP_SHADOW_OFFSET, scale = 10)

    display.set_pen(WHITE)
    display.text(NAME, ((WIDTH - text_width) // 2), HEIGHT // 2 - text_height//2, scale=10)

    # update positions for each logo and display it while accounting for edge and object collisions
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
