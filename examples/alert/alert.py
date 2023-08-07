"""
A spaceship alert panel, with spurious graphs, 'radar' and noise. Used with permission of Pimoroni Mining Corporation.

Press the Y button to cycle through the alert modes.

You will need the future_earth.af font saved to your PicoVision alongside this script.
Font originally from https://www.dafont.com/future-earth.font
"""

from modes import VGA
from picovector import PicoVector, ANTIALIAS_X16, Polygon
from pimoroni import Button
import random
from picosynth import PicoSynth, Channel

# list of alert modes
MODES = ["NOT ALERT", "RED ALERT", "YELLOW ALERT", "BLUE ALERT", "RAINBOW ALERT"]
# ships you might encounter in your voyages
SHIPS = ["GELF_SHIP", "BORG_CUBE", "STAR_DEST", "VORLONS", "BASE_STAR", "TARDIS", "THARGOID", "PLANET_X", "JWST", "ELONS_TESLA", "PLUTO", "THE_REVENGE", "BLACK_PEARL"]
PULSE_SPEED = 0.2
RAINBOW_SPEED = 0.04
SIREN_URGENCY = 8
VOLUME = 0.5

# set up hardware
graphics = VGA()

vector = PicoVector(graphics)

WHITE = graphics.create_pen(255, 255, 255)
BLACK = graphics.create_pen(0, 0, 0)
RED = graphics.create_pen(255, 0, 0)
GREEN = graphics.create_pen(32, 84, 36)

WIDTH, HEIGHT = graphics.get_bounds()

synth = PicoSynth()

siren = synth.channel(0)
siren.configure(
    waveforms=Channel.SQUARE | Channel.TRIANGLE,
    attack=0.1,
    decay=0.1,
    sustain=0.0,
    release=0.5,
    volume=VOLUME
)

synth.play()

button_y = Button(9)


def make_starfield():
    # populates the starfield with random stars and ships
    global stars, pmc, ship
    stars = []
    pmc = (random.randint(0, WIDTH - 50), random.randint(HEIGHT - 160, HEIGHT - 120))
    for i in range(90):
        stars.append((random.randint(8, WIDTH - 8), random.randint(HEIGHT - 180, HEIGHT - 100)))
    ship = (random.randint(0, WIDTH - 50), random.randint(HEIGHT - 160, HEIGHT - 120), random.choice(SHIPS))


def draw_starfield():
    # draws the starfield and ships
    global stars, pmc
    graphics.set_pen(BLACK)
    graphics.rectangle(0, HEIGHT - 185, WIDTH, HEIGHT - 95)
    graphics.set_pen(GREEN)
    for s in stars:
        graphics.circle(s[0], s[1], 1)
    graphics.set_pen(WHITE)
    graphics.triangle(pmc[0], pmc[1], pmc[0] - 12, pmc[1] + 8, pmc[0] - 12, pmc[1] - 8)
    graphics.text("PMC_ship", pmc[0] + 4, pmc[1] + 4, scale=1)
    graphics.set_pen(RED)
    graphics.circle(ship[0], ship[1], 3)
    graphics.text(ship[2], ship[0] + 4, ship[1] + 4, scale=1)


def full_refresh():
    global pulse, frequency
    # redraws the border and starfield
    # drawing big polygons is resource intensive, so we only want to do it when we switch modes
    # first refresh the starfield
    make_starfield()
    pulse = 0.0
    frequency = 440
    # draw the elements that we don't need to redraw every time
    # we're drawing them twice, so they end up in both buffers
    for i in range(2):
        # clear the screen
        graphics.set_pen(BLACK)
        graphics.clear()
        graphics.set_pen(WHITE)
        # draw a border, with vectors
        vector.draw(OUTER_RECTANGLE, INNER_RECTANGLE)
        graphics.set_pen(BLACK)
        vector.set_font("/future-earth.af", 30)
        vector.text("ALERT STATUS", 50, -15)
        draw_starfield()
        graphics.update()


def rectangle(x, y, w, h, cx=0, cy=0):
    # returns a polygon representing a rectangle, with optional cutoff corners
    points = []

    if cx > 0 and cy > 0:
        points.append((x, y + cy))
        points.append((x + cx, y))

        points.append((x + w - cx, y))
        points.append((x + w, y + cy))

        points.append((x + w, y + h - cy))
        points.append((x + w - cx, y + h))

        points.append((x + cx, y + h))
        points.append((x, y + h - cy))
    else:
        points.append((x, y))
        points.append((x + w, y))
        points.append((x + w, y + h))
        points.append((x, y + h))

    return Polygon(*points)


# the shapes we'll use for the border
BORDER_THICKNESS = 18
OUTER_RECTANGLE = rectangle(0, 0, WIDTH, int(HEIGHT * 0.6), 40, 40)
INNER_RECTANGLE = rectangle(BORDER_THICKNESS, BORDER_THICKNESS, WIDTH - (BORDER_THICKNESS * 2), int(HEIGHT * 0.6) - (BORDER_THICKNESS * 2), 30, 30)

# variables
stars = []
pmc = (0, 0)  # for the Pimoroni Mining Corporation ship
ship = (0, 0, "")
text_hue = 0.0
graph_hue = 0.0
pulse = 0.0
pulse_direction = PULSE_SPEED
graph_data = []
frequency = 440

# setup
current_mode = MODES[0]
# populate the graph data list with 12 random values between 1 and 100
for i in range(12):
    graph_data.append(random.randint(1, 100))
vector.set_antialiasing(ANTIALIAS_X16)
full_refresh()

while True:
    if button_y.is_pressed:
        # cycle through the modes
        current_mode = MODES[(MODES.index(current_mode) + 1) % len(MODES)]
        full_refresh()

    # just clear the areas with text / graphs, for faster drawing
    graphics.set_pen(BLACK)
    graphics.rectangle(75, 60, 500, 55)
    graphics.rectangle(75, 180, 340, 55)
    graphics.rectangle(0, HEIGHT - 100, WIDTH, 100)

    # draw the text
    vector.set_font("/future-earth.af", 120)
    # static
    if current_mode == "NOT ALERT":
        graphics.set_pen(WHITE)
        vector.text(current_mode, 80, -10)
    # increment the text hue (for rainbow mode)
    elif current_mode == "RAINBOW ALERT":
        text_hue += RAINBOW_SPEED
        graphics.set_pen(graphics.create_pen_hsv(text_hue, 1.0, 1.0))
        vector.text(current_mode, 80, -10)
    else:
        # pulse the text
        pulse += pulse_direction
        if pulse >= 1.0:
            pulse = 1.0
            pulse_direction = -PULSE_SPEED
        elif pulse <= 0.0:
            pulse = 0.0
            pulse_direction = PULSE_SPEED

        if current_mode == "RED ALERT":
            text_hue = 0 / 360
        elif current_mode == "YELLOW ALERT":
            text_hue = 60 / 360
        elif current_mode == "BLUE ALERT":
            text_hue = 230 / 360
        graphics.set_pen(graphics.create_pen_hsv(text_hue, 1.0, pulse))
        vector.text(current_mode, 80, -10)

    # draw spurious graphs
    # default colours for the graph
    graph_hue_start = 120 / 360  # green
    graph_hue_end = 0 / 360  # red
    # override the default colours
    if current_mode == "BLUE ALERT":
        graph_hue_start = 180 / 360
        graph_hue_end = 230 / 360
    # adjust the graph data
    for i in range(len(graph_data)):
        # adjust value up or down
        graph_data[i] += random.randint(-1, 1)
        if current_mode == "NOT ALERT":
            # everything is ok, keep values between 0 and 30
            graph_data[i] = max(0, min(graph_data[i], 20))
        elif current_mode == "RED ALERT":
            # everything is bad, keep values between 80 and 100
            graph_data[i] = max(80, min(graph_data[i], 100))
        elif current_mode == "YELLOW ALERT":
            # everything is yellow, keep values between 40 and 70
            graph_data[i] = max(40, min(graph_data[i], 70))
        else:
            # keep value between 0 and 100
            graph_data[i] = max(0, min(graph_data[i], 100))

        if current_mode == "RAINBOW ALERT":
            # use the text colour for the graphs
            graphics.set_pen(graphics.create_pen_hsv(text_hue, 1.0, 1.0))
        else:
            # calculate a hue based on the value
            graph_hue = graph_hue_start + ((graph_data[i]) * (graph_hue_end - graph_hue_start) / 100)
            # make sure hue is between 0 and 1
            graph_hue = max(0.0, min(graph_hue, 1.0))
            graphics.set_pen(graphics.create_pen_hsv(graph_hue, 1.0, 1.0))
        # draw the bars at the bottom of the screen
        graphics.rectangle(int(WIDTH / 12 * i), HEIGHT - graph_data[i], 50, graph_data[i])

    # make some noise
    if current_mode == "RED ALERT":
        # make a sireny noise
        siren.frequency(frequency)
        siren.trigger_attack()
        frequency += SIREN_URGENCY
        if frequency > 540:
            frequency = 440
    else:
        # beepety boopety
        # pick a random number from the graph data
        frequency = random.choice(graph_data) * 8
        # check frequency is not below 1
        frequency = max(1, frequency)
        siren.frequency(frequency)
        siren.trigger_attack()

    graphics.update()
