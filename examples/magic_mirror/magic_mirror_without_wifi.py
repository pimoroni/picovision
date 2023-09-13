"""
MicroMagicMirror - a basic home dashboard that shows the time and a graph from the Pico's internal temperature sensor.
"""

from modes import VGA
import time
import machine

# Constants for drawing the display
TITLE = "MicroMagicMirror"
TITLE_POSITION = 20
ROW_1_POSITION = 72
COLUMN_2_POSITION = 80
CLOCK_POSITION = 430
PADDING = 10
GRAPH_TOP = 120
GRAPH_BOTTOM = 380

# set up our display and some useful constants
graphics = VGA()

WIDTH, HEIGHT = graphics.get_bounds()

BLACK = graphics.create_pen(0, 0, 0)
RED = graphics.create_pen(255, 0, 0)
ORANGE = graphics.create_pen(246, 138, 30)
YELLOW = graphics.create_pen(255, 216, 0)
GREEN = graphics.create_pen(0, 121, 64)
BLUE = graphics.create_pen(0, 0, 255)
INDIGO = graphics.create_pen(36, 64, 142)
VIOLET = graphics.create_pen(115, 41, 130)
WHITE = graphics.create_pen(255, 255, 255)

# create the rtc object
rtc = machine.RTC()

sensor_temp = machine.ADC(4)
conversion_factor = 3.3 / (65535)  # used for calculating a temperature from the raw sensor reading

# some other variables for keeping track of stuff
timer = 0
second = 0
last_second = 0
last_x = 0
last_y = 0

temperatures = []


def redraw_display_if_reqd():
    global year, month, day, wd, hour, minute, second, last_second, last_x, last_y
    if second != last_second:
        # clear the screen
        graphics.set_pen(BLACK)
        graphics.clear()

        # draw a lovely rainbow
        graphics.set_pen(VIOLET)
        graphics.circle(0, 0, 140)
        graphics.set_pen(BLUE)
        graphics.circle(0, 0, 120)
        graphics.set_pen(GREEN)
        graphics.circle(0, 0, 100)
        graphics.set_pen(YELLOW)
        graphics.circle(0, 0, 80)
        graphics.set_pen(ORANGE)
        graphics.circle(0, 0, 60)
        graphics.set_pen(RED)
        graphics.circle(0, 0, 40)

        # draw the title
        graphics.set_pen(WHITE)
        graphics.text(TITLE, COLUMN_2_POSITION, TITLE_POSITION, scale=7)

        # show system temperature
        # the following two lines do some maths to convert the number from the temp sensor into celsius
        reading = sensor_temp.read_u16() * conversion_factor
        internal_temperature = 27 - (reading - 0.706) / 0.001721
        graphics.text(f"uMM is running at {internal_temperature:.1f}°C.", COLUMN_2_POSITION, ROW_1_POSITION, WIDTH - COLUMN_2_POSITION, scale=2)

        # draw a graph of the last 60 temperature readings
        temperatures.append(internal_temperature)
        if len(temperatures) > 61:
            temperatures.pop(0)

        # draw a box for the graph
        graphics.set_pen(WHITE)
        graphics.line(PADDING, GRAPH_TOP, WIDTH - PADDING, GRAPH_TOP)
        graphics.line(PADDING, GRAPH_BOTTOM, WIDTH - PADDING, GRAPH_BOTTOM)
        graphics.line(PADDING, GRAPH_TOP, PADDING, GRAPH_BOTTOM)
        graphics.line(WIDTH - PADDING, GRAPH_TOP, WIDTH - PADDING, GRAPH_BOTTOM)

        # scale the graph to fit the screen
        max_temp = max(temperatures) + 2
        min_temp = min(temperatures) - 2

        # draw the scale
        graphics.text(f"{max_temp:.1f}°C", PADDING, GRAPH_TOP - 15, scale=2)
        graphics.text(f"{min_temp:.1f}°C", PADDING, GRAPH_BOTTOM + 3, scale=2)

        # draw dots and join them with a line
        graphics.set_pen(RED)
        for i, temp in enumerate(temperatures):
            x = PADDING + i * (WIDTH - PADDING * 2) // 60
            y = GRAPH_BOTTOM - (temp - min_temp) / (max_temp - min_temp) * (GRAPH_BOTTOM - GRAPH_TOP)
            if i > 0:
                graphics.line(int(x), int(y), int(last_x), int(last_y), 3)
            last_x = x
            last_y = y

        # draw clock
        graphics.set_pen(WHITE)
        graphics.text(f"{hour:02}:{minute:02}:{second:02}", PADDING, CLOCK_POSITION, scale=7)
        text_width = graphics.measure_text(f"{day}/{month}/{year}", scale=7)
        graphics.text(f"{day}/{month}/{year}", WIDTH - text_width, CLOCK_POSITION, scale=7)

        last_second = second
        graphics.update()


graphics.set_font("bitmap8")

redraw_display_if_reqd()

while True:
    ticks_now = time.ticks_ms()

    year, month, day, wd, hour, minute, second, _ = rtc.datetime()

    redraw_display_if_reqd()
