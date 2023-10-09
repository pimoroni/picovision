# A customisable Pride flag. If you want to be a cool crab like @Foone you can add an image on top of the flag.
# Need to generate a pride flag in MS-DOS? Check out https://github.com/foone/VGAPride !

from modes import VGA
import math
import pngdec

display = VGA()

# Create a new PNG decoder for our PicoGraphics
png = pngdec.PNG(display)

WIDTH, HEIGHT = display.get_bounds()

# If you want to add a PNG image, add the filename here
IMAGE = "/pride/coolcrab.png"

# List of available pen colours, add more if necessary
RED = display.create_pen(209, 34, 41)
ORANGE = display.create_pen(246, 138, 30)
YELLOW = display.create_pen(255, 216, 0)
GREEN = display.create_pen(0, 121, 64)
INDIGO = display.create_pen(36, 64, 142)
VIOLET = display.create_pen(115, 41, 130)
WHITE = display.create_pen(255, 255, 255)
PINK = display.create_pen(255, 175, 200)
BLUE = display.create_pen(116, 215, 238)
BROWN = display.create_pen(97, 57, 21)
BLACK = display.create_pen(0, 0, 0)
MAGENTA = display.create_pen(255, 33, 140)
CYAN = display.create_pen(33, 177, 255)
AMETHYST = display.create_pen(156, 89, 209)

# Uncomment one of these to change flag
# If adding your own, colour order is left to right (or top to bottom)
COLOUR_ORDER = [RED, ORANGE, YELLOW, GREEN, INDIGO, VIOLET]  # traditional pride flag
# COLOUR_ORDER = [BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, INDIGO, VIOLET]  # Philadelphia pride flag
# COLOUR_ORDER = [BLUE, PINK, WHITE, PINK, BLUE]  # trans flag
# COLOUR_ORDER = [MAGENTA, YELLOW, CYAN]  # pan flag
# COLOUR_ORDER = [MAGENTA, VIOLET, INDIGO]  # bi flag
# COLOUR_ORDER = [YELLOW, WHITE, AMETHYST, BLACK]  # non-binary flag

# Add chevrons to the left
# CHEVRONS = [] # No chevrons
CHEVRONS = [WHITE, PINK, BLUE, BROWN, BLACK]  # Progress Pride Flag
# Initial chevron height compared to screen height
FIRST_CHEVRON_HEIGHT = 0.4

# Change this for vertical stripes
STRIPES_DIRECTION = "horizontal"

# Draw the flag
if STRIPES_DIRECTION == "horizontal":
    stripe_width = round(HEIGHT / len(COLOUR_ORDER))
    for x in range(len(COLOUR_ORDER)):
        display.set_pen(COLOUR_ORDER[x])
        display.rectangle(0, stripe_width * x, WIDTH, stripe_width)

if STRIPES_DIRECTION == "vertical":
    stripe_width = round(WIDTH / len(COLOUR_ORDER))
    for x in range(len(COLOUR_ORDER)):
        display.set_pen(COLOUR_ORDER[x])
        display.rectangle(stripe_width * x, 0, stripe_width, HEIGHT)

if len(CHEVRONS) > 0:
    stripe_width = round((HEIGHT * (1 - FIRST_CHEVRON_HEIGHT)) / len(CHEVRONS))
    offset = -stripe_width * math.floor((len(CHEVRONS) + 1) / 2)
    middle = round(HEIGHT / 2)
    for x in range(len(CHEVRONS) - 1, -1, -1):
        display.set_pen(CHEVRONS[x])
        display.triangle(
            x * stripe_width + offset, -stripe_width,
            (x + 1) * stripe_width + offset + middle, middle,
            x * stripe_width + offset, HEIGHT + stripe_width)

try:
    png.open_file(IMAGE)
    # cool crab is 500 x 255px - if your image is a different size you might need to tweak the position
    png.decode(WIDTH // 2 - (500 // 2), HEIGHT // 2 - (255 // 2))
except OSError as e:
    print(f"{e} - image not found.")

# Once all the drawing is done, update the display.
display.update()
