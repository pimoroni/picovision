import time
import gc

from picographics import PicoGraphics, PEN_RGB555
from picovector import PicoVector, Polygon, RegularPolygon, Rectangle, ANTIALIAS_X16

# machine.freq(168_000_000)

display = PicoGraphics(PEN_RGB555, 720, 480)

vector = PicoVector(display)
vector.set_antialiasing(ANTIALIAS_X16)

RED = display.create_pen(200, 0, 0)
BLACK = display.create_pen(0, 0, 0)
GREY = display.create_pen(200, 200, 200)
WHITE = display.create_pen(255, 255, 255)

"""
# Redefine colours for a Blue clock
RED = display.create_pen(200, 0, 0)
BLACK = display.create_pen(135, 159, 169)
GREY = display.create_pen(10, 40, 50)
WHITE = display.create_pen(14, 60, 76)
"""

WIDTH, HEIGHT = display.get_bounds()

hub = RegularPolygon(int(WIDTH / 2), int(HEIGHT / 2), 24, 5)

face = RegularPolygon(int(WIDTH / 2), int(HEIGHT / 2), 48 * 2, int(HEIGHT / 2), (360.0 / 48))

print(time.localtime())

# Pre-calculate all the polygons for the clock face tick marks
ticks = []
tick_shadows = []

for a in range(60):
    if a % 5 == 0:
        continue

    tick_mark = Rectangle(int(WIDTH / 2) - 3, 10, 6, int(HEIGHT / 48))
    vector.rotate(tick_mark, 360 / 60.0 * a, int(WIDTH / 2), int(HEIGHT / 2))

    tick_shadow = Rectangle(int(WIDTH / 2) - 3, 10, 6, int(HEIGHT / 48))
    vector.rotate(tick_shadow, 360 / 60.0 * a, int(WIDTH / 2), int(HEIGHT / 2))
    vector.translate(tick_shadow, 0, 2)

    ticks.append(tick_mark)
    tick_shadows.append(tick_shadow)

for a in range(12):
    hour_mark = Rectangle(int(WIDTH / 2) - 5, 10, 10, int(HEIGHT / 10))
    vector.rotate(hour_mark, 360 / 12.0 * a, int(WIDTH / 2), int(HEIGHT / 2))

    hour_shadow = Rectangle(int(WIDTH / 2) - 5, 10, 10, int(HEIGHT / 10))
    vector.rotate(hour_shadow, 360 / 12.0 * a, int(WIDTH / 2), int(HEIGHT / 2))
    vector.translate(hour_shadow, 0, 2)

    ticks.append(hour_mark)
    tick_shadows.append(hour_shadow)


# Set the background colour and draw the clock face and tick marks into both PSRAM buffers
for _ in range(2):
    display.set_pen(BLACK)
    display.clear()
    display.set_pen(WHITE)
    vector.draw(face)

    display.set_pen(GREY)

    for shadow in tick_shadows:
        vector.draw(shadow)

    display.set_pen(BLACK)

    for tick in ticks:
        vector.draw(tick)

    display.update()


t_frames = 0
t_total = 0
last_second = None

last_clip = [(0, 0, 0, 0), (0, 0, 0, 0)]

while True:
    year, month, day, hour, minute, second, _, _ = time.localtime()

    if last_second != second:
        t_sec = time.ticks_ms()
        last_second = second

    t_start = time.ticks_ms()

    second_frac = (t_start - t_sec) / 1000.0
    angle_second = (second + second_frac) * 6
    second_hand_length = int(HEIGHT / 2) - int(HEIGHT / 8)
    second_hand = Polygon((-2, -second_hand_length), (-2, int(HEIGHT / 8)), (2, int(HEIGHT / 8)), (2, -second_hand_length))
    vector.rotate(second_hand, angle_second, 0, 0)
    vector.translate(second_hand, int(WIDTH / 2), int(HEIGHT / 2) + 5)

    angle_minute = minute * 6
    angle_minute += (second + second_frac) / 10.0
    minute_hand_length = int(HEIGHT / 2) - int(HEIGHT / 24)
    minute_hand = Polygon((-5, -minute_hand_length), (-10, int(HEIGHT / 16)), (10, int(HEIGHT / 16)), (5, -minute_hand_length))
    vector.rotate(minute_hand, angle_minute, 0, 0)
    vector.translate(minute_hand, int(WIDTH / 2), int(HEIGHT / 2) + 5)

    hour_frac = (second + second_frac) / 60
    angle_hour = (hour % 12) * 30
    angle_hour += (minute + hour_frac) / 2
    hour_hand_length = int(HEIGHT / 2) - int(HEIGHT / 8)
    hour_hand = Polygon((-5, -hour_hand_length), (-10, int(HEIGHT / 16)), (10, int(HEIGHT / 16)), (5, -hour_hand_length))
    vector.rotate(hour_hand, angle_hour, 0, 0)
    vector.translate(hour_hand, int(WIDTH / 2), int(HEIGHT / 2) + 5)

    second_hand_bounds = second_hand.bounds()
    minute_hand_bounds = minute_hand.bounds()
    hour_hand_bounds = hour_hand.bounds()

    # Create a clipping region which includes all of the hands
    clip_x = min(second_hand_bounds[0], minute_hand_bounds[0], hour_hand_bounds[0])
    clip_y = min(second_hand_bounds[1], minute_hand_bounds[1], hour_hand_bounds[1]) - 5
    clip_max_x = max(second_hand_bounds[0] + second_hand_bounds[2], minute_hand_bounds[0] + minute_hand_bounds[2], hour_hand_bounds[0] + hour_hand_bounds[2])
    clip_max_y = max(second_hand_bounds[1] + second_hand_bounds[3], minute_hand_bounds[1] + minute_hand_bounds[3], hour_hand_bounds[1] + hour_hand_bounds[3]) + 5

    # Remember this clipping window for 2 frames time
    last_clip.append((clip_x, clip_y, clip_max_x, clip_max_y))

    # Expand to also cover the clipping window from 2 frames ago
    clip_x = min(clip_x, last_clip[0][0])
    clip_y = min(clip_y, last_clip[0][1])
    clip_max_x = max(clip_max_x, last_clip[0][2])
    clip_max_y = max(clip_max_y, last_clip[0][3])

    # Apply the clipping region so we only redraw the parts of the clock which have changed
    display.set_clip(clip_x, clip_y, clip_max_x - clip_x + 1, clip_max_y - clip_y + 1)

    # Drop the clipping window from 2 frames ago, we no longer need it
    last_clip = last_clip[1:]

    # Clear to white just inside our anti-aliased outer circle and clipping region
    display.set_pen(WHITE)
    display.circle(int(WIDTH / 2), int(HEIGHT / 2), int(HEIGHT / 2) - 4)

    # Draw the shadows
    display.set_pen(GREY)

    for shadow in tick_shadows:
        vector.draw(shadow)

    vector.draw(minute_hand)
    vector.draw(hour_hand)
    vector.draw(second_hand)

    # Draw the face
    display.set_pen(BLACK)

    for tick in ticks:
        vector.draw(tick)

    vector.translate(minute_hand, 0, -5)
    vector.translate(hour_hand, 0, -5)
    vector.draw(minute_hand)
    vector.draw(hour_hand)

    display.set_pen(RED)
    vector.translate(second_hand, 0, -5)
    vector.draw(second_hand)
    vector.draw(hub)

    t_end = time.ticks_ms()
    t_total += t_end - t_start
    t_frames += 1

    if t_frames == 60:
        f_avg = t_total / 60.0
        f_fps = 1000.0 / f_avg
        print(f"Took {t_total}ms for 60 frames, {f_avg:.02f}ms avg. {f_fps:.02f}FPS avg.")
        t_frames = 0
        t_total = 0

    display.update()
    gc.collect()
