# Travel through a Windows 3.1-esque starfield, with stars growing as they get 'closer'.
# This version is rainbow (we didn't get that in 1992!)

from picographics import PicoGraphics, PEN_RGB555
import random

# Constants to play with
NUMBER_OF_STARS = 150
TRAVEL_SPEED = 1.2
STAR_GROWTH = 0.20
HUE_SHIFT = 0.05

# Set up our display
graphics = PicoGraphics(pen_type=PEN_RGB555, width=640, height=480)

WIDTH, HEIGHT = graphics.get_bounds()

BLACK = graphics.create_pen(0, 0, 0)
WHITE = graphics.create_pen(255, 255, 255)


def new_star():
    # Create a new star, with initial x, y, size and hue
    # Initial x will fall between -WIDTH / 2 and +WIDTH / 2 and y between -HEIGHT/2 and +HEIGHT/2
    # These are relative values for now, treating (0, 0) as the centre of the screen.
    return [random.randint(0, WIDTH) - WIDTH // 2, random.randint(0, HEIGHT) - HEIGHT // 2, 1.0, random.random()]


stars = [new_star() for _ in range(NUMBER_OF_STARS)]

while True:
    graphics.set_pen(BLACK)
    graphics.clear()
    for i in range(NUMBER_OF_STARS):
        # Load a star from the stars list
        s = stars[i]

        # Update x
        s[0] = s[0] * TRAVEL_SPEED

        # Update y
        s[1] = s[1] * TRAVEL_SPEED

        if s[0] <= - WIDTH // 2 or s[0] >= WIDTH // 2 or s[1] <= - HEIGHT // 2 or s[1] >= HEIGHT // 2 or s[2] >= 10:
            # This star has fallen off the screen (or rolled dead centre and grown too big!)
            # Replace it with a new one
            s = new_star()

        # Grow the star as it travels outward
        s[2] += STAR_GROWTH

        # Adjust the hue
        s[3] += HUE_SHIFT

        # Save the updated star to the list
        stars[i] = s

        # Set the pen colour based on the star's hue
        graphics.set_pen(graphics.create_pen_hsv(s[3], 1.0, 1.0))

        # Draw star, adding offsets to our relative coordinates to allow for (0, 0) being in the top left corner.
        graphics.circle(int(s[0]) + WIDTH // 2, int(s[1]) + HEIGHT // 2, int(s[2]))
    graphics.update()
