import time
import math
import random
import machine
import gc
import cppmem
import pngdec
from picographics import PicoGraphics, DISPLAY_PICOVISION, PEN_DV_RGB555
from picovector import PicoVector, Polygon, ANTIALIAS_X4


cppmem.set_mode(cppmem.MICROPYTHON)

display = PicoGraphics(DISPLAY_PICOVISION, width=720, height=480, frame_width=720 * 2, frame_height=480, pen_type=PEN_DV_RGB555)
png = pngdec.PNG(display)

SPRITE_PIPE = 0
SPRITE_PIPE_CAP = 1
SPRITE_BIRB_1 = 2
SPRITE_BIRB_2 = 3
SPRITE_BIRB_3 = 4
BIRBS = [2, 3, 4]
SPRITE_GOAL = 5

for _ in range(2):
    display.load_sprite("pipe.png", SPRITE_PIPE)
    display.load_sprite("goal.png", SPRITE_GOAL)
    display.load_sprite("pipe-cap.png", SPRITE_PIPE_CAP)
    display.load_sprite("birb-sprite.png", SPRITE_BIRB_1, (0, 32, 32, 32))
    display.load_sprite("birb-sprite.png", SPRITE_BIRB_2, (32, 32, 32, 32))
    display.load_sprite("birb-sprite.png", SPRITE_BIRB_3, (64, 32, 32, 32))
    display.update()

vector = PicoVector(display)
vector.set_antialiasing(ANTIALIAS_X4)
vector.set_font("OpenSans-Regular.af", 50)


# SKY = display.create_pen(0x74, 0xc6, 0xd4)
SKY = display.create_pen(0xb0, 0xda, 0xe1)
PIPE = display.create_pen(0x79, 0xdb, 0x2c)
PIPE_SHADOW = display.create_pen(0x69, 0xab, 0x0c)     # 69ab0c
PIPE_HIGHLIGHT = display.create_pen(0xe1, 0xfe, 0x87)  # e1fe87
PIPE_LIGHT = display.create_pen(0x9d, 0xe8, 0x5a)      # 9de85a
GROUND = display.create_pen(0xde, 0xd9, 0x95)
BLACK = display.create_pen(0x0e, 0x09, 0x05)
CLOUD = display.create_pen(0xec, 0xfc, 0xdd)
GRASS = display.create_pen(0x7d, 0xe4, 0x8c)


WIDTH, HEIGHT = display.get_bounds()

GAME_WIDTH = int(WIDTH / 2)
GAME_HEIGHT = int(HEIGHT / 3) * 2
CLOUD_HEIGHT = 100

SLICES = 10
SLICE_WIDTH = int(WIDTH / SLICES)
OFFSCREEN_SLICES = int(SLICES / 2)

PIPE_WIDTH = 20
CAP_WIDTH = 26
CAP_HEIGHT = 15

N_SCREENS = 2
LEVEL_SIZE = N_SCREENS * SLICES

GAME_TOP = int((HEIGHT - GAME_HEIGHT) / 2)
GAME_BOTTOM = GAME_TOP + GAME_HEIGHT

GAME_GET_READY = 0
GAME_RUNNING = 1
GAME_WIN = 2
GAME_LOSE = 3

COUNTDOWN_TIME_MS = 3000  # ms

# Gap Y positions for 128 slices
PIPE_GAPS = [0 for _ in range(LEVEL_SIZE)]
GAP_WIDTHS = [0 for _ in range(LEVEL_SIZE)]
SCORE = [False for _ in range(LEVEL_SIZE)]

game_state = GAME_GET_READY
t_start = 0


for _ in range(2):
    # Scroll position two for fixed top/bottom
    display.set_scroll_index_for_lines(2, 0, GAME_TOP - 20)
    display.set_scroll_index_for_lines(2, GAME_BOTTOM + 20, HEIGHT)

    # For scrolling borders along the top/bottom
    display.set_scroll_index_for_lines(1, GAME_TOP - 20, GAME_TOP)
    display.set_scroll_index_for_lines(1, GAME_BOTTOM, GAME_BOTTOM + 20)

    display.set_scroll_index_for_lines(3, GAME_BOTTOM - CLOUD_HEIGHT, GAME_BOTTOM)
    display.update()


def new_level():
    for n in range(LEVEL_SIZE):
        MAX_VARIANCE = 110
        gap_variance = int(min(MAX_VARIANCE, math.log(n + 1, 2) * 20))
        GAP_WIDTH = random.randint(40 + MAX_VARIANCE - gap_variance, 200 - gap_variance)
        # print(f"{n} {GAP_WIDTH} {gap_variance}")
        GAP_WIDTH = min(190, GAP_WIDTH)
        GAP_WIDTHS[n] = GAP_WIDTH

        GAP_ZONE_START = 50 + CAP_HEIGHT + int(GAP_WIDTH / 2)
        GAP_ZONE_END = GAME_HEIGHT - 50 - CAP_HEIGHT - int(GAP_WIDTH / 2)
        GAP_ZONE_HEIGHT = GAP_ZONE_END - GAP_ZONE_START

        if n & 1:
            PIPE_GAPS[n] = random.randint(
                GAP_ZONE_START,
                GAP_ZONE_END - int(GAP_ZONE_HEIGHT / 2)
            )
        else:
            PIPE_GAPS[n] = random.randint(
                GAP_ZONE_START + int(GAP_ZONE_HEIGHT / 2),
                GAP_ZONE_END
            )


def draw_score():
    display.set_clip(0, 0, WIDTH, GAME_TOP - 20)
    display.set_pen(GROUND)
    display.rectangle(0, 0, GAME_WIDTH, GAME_TOP - 20)
    display.set_pen(PIPE_SHADOW)
    vector.text(f"Score: {sum(SCORE)}", 15, -5)
    display.set_clip(0, 0, WIDTH, HEIGHT)


def draw_info(text):
    display.set_clip(0, 0, WIDTH, GAME_TOP - 20)
    display.set_pen(GROUND)
    display.rectangle(0, 0, GAME_WIDTH, GAME_TOP - 20)
    display.set_pen(PIPE_SHADOW)
    vector.text(text, 15, -5)
    display.set_clip(0, 0, WIDTH, HEIGHT)


def outline(x, y, w, h):
    w -= 1

    display.line(x, y, x + w, y)
    display.line(x, y + h, x + w, y + h)

    display.line(x, y, x, y + h)
    display.line(x + w, y, x + w, y + h)


def prep_game():
    # Prep the backdrop
    png.open_file("pimoroni-hq.png")
    for display_index in range(2):
        display.set_pen(GROUND)
        display.clear()
        display.set_pen(BLACK)
        display.rectangle(0, GAME_TOP - 20, WIDTH, GAME_HEIGHT + 40)

        # Stripy pipes above and below the playing field
        display.set_pen(PIPE)
        display.rectangle(0, GAME_TOP - 18, WIDTH, 16)
        display.rectangle(0, GAME_BOTTOM + 2, WIDTH, 16)

        display.set_pen(PIPE_SHADOW)
        chevron_spacing = 20
        for x in range(int(WIDTH / chevron_spacing)):
            o_x = x * chevron_spacing
            o_y = GAME_TOP - 18
            vector.draw(Polygon((o_x + 10, o_y), (o_x + 20, o_y), (o_x + 10, o_y + 16), (o_x, o_y + 16)))
            o_y += GAME_HEIGHT + 20
            vector.draw(Polygon((o_x + 10, o_y), (o_x + 20, o_y), (o_x + 10, o_y + 16), (o_x, o_y + 16)))

        display.set_pen(SKY)
        display.rectangle(0, GAME_TOP, WIDTH, GAME_HEIGHT - CLOUD_HEIGHT)

        # The grass, cloud art will overlap this
        display.set_pen(GRASS)
        display.rectangle(0, GAME_BOTTOM - CLOUD_HEIGHT, WIDTH, CLOUD_HEIGHT)

        png.decode(0, GAME_BOTTOM - CLOUD_HEIGHT)

        display.set_clip(0, 0, WIDTH, HEIGHT)

        draw_score()

        display.update()


def reset_game():
    global t_start, game_state

    for x in range(LEVEL_SIZE):
        SCORE[x] = False

    new_state(GAME_RUNNING)

    birb.reset()

    for display_index in range(2):
        draw_score()
        display.update()


class SpriteList:
    def __init__(self):
        self.items = []

    def add(self, image, x, y, force=False):
        if len(self.items) == 32:
            if force:
                self.items.pop(0)
            else:
                return
        self.items.append((image, x, y))

    def clear(self):
        self.items = []

    def display(self):
        for i in range(32):
            try:
                image, x, y = self.items[i]
                display.display_sprite(i, image, x, y)
            except IndexError:
                display.clear_sprite(i)


class CollisionList:
    def __init__(self):
        self.zones = []

    def add(self, zone, action, **args):
        self.zones.append((zone, action, args))

    def clear(self):
        self.zones = []

    def test(self, x, y, w, h):
        for zone, action, args in self.zones:
            zx, zy, zw, zh = zone
            if not (x > zx + zw or x + w < zx or y > zy + zh or y + h < zy):
                action(**args)

    def debug(self, offset_x):
        for _ in range(2):
            for zone, action in self.zones:
                zx, zy, zw, zh = zone
                display.set_pen(display.create_pen(255, 0, 0))
                display.rectangle(zx + offset_x, zy, zw, zh)
            display.update()


class Birb:
    def __init__(self, pin):
        self.reset()

        self.button = machine.Pin(pin, machine.Pin.IN, machine.Pin.PULL_UP)

    def reset(self):
        self.x = int(WIDTH / 6)
        self.y = GAME_TOP + int(GAME_HEIGHT / 2) - 16
        self.vy = 0.5

    def update(self):
        self.vy += 0.98
        self.vy *= 0.90

        if self.button.value() == 0:
            self.vy -= 2

        self.y += self.vy

        if self.y > GAME_BOTTOM - 32:
            self.y = GAME_BOTTOM - 32
            self.vy -= 2

        if self.y < GAME_TOP:
            self.y = GAME_TOP
            self.vy *= -0.5

    def bounds(self):
        return self.x + 4, self.y + 8, 32 - 8, 32 - 16


def new_state(state):
    global game_state, t_start
    t_start = time.ticks_ms()
    game_state = state


def game_over():
    new_state(GAME_LOSE)
    print(f"Ouch! {time.ticks_ms()}")


def game_win():
    new_state(GAME_WIN)
    print(f"You win! {time.ticks_ms()}")


def score_point(offset=None):
    try:
        update = not SCORE[offset]
        SCORE[offset] = True
        if update:
            draw_score()
            display.update()
    except IndexError:
        pass


def main_game_running(t_current):
    global end_index

    current_x = int(t_current / 20.0)
    cloud_x = int(t_current / 400.0)

    birb.update()

    level_offset = int(current_x / SLICE_WIDTH)

    display.set_display_offset(current_x % GAME_WIDTH, 0, 1)
    display.set_display_offset(cloud_x % GAME_WIDTH, 0, 3)

    spritelist.clear()

    for pipe in range(0, int(SLICES / 2) + 1):
        try:
            gap = PIPE_GAPS[level_offset + pipe]
            gap_width = GAP_WIDTHS[level_offset + pipe]
        except IndexError:
            gap = int(GAME_HEIGHT / 2)
            gap_width = GAME_HEIGHT - 100 - CAP_HEIGHT - CAP_HEIGHT
            if end_index is None:
                end_index = level_offset + pipe + 2

        top_pipe_height = gap - int(gap_width / 2)
        bottom_pipe_height = GAME_HEIGHT - top_pipe_height - gap_width

        top_pipe_end = GAME_TOP + top_pipe_height
        bottom_pipe_start = top_pipe_end + gap_width

        pipe_x = pipe * SLICE_WIDTH - (current_x % SLICE_WIDTH)
        pipe_x += int(SLICE_WIDTH / 2) - 10

        if pipe_x <= -CAP_WIDTH or pipe_x >= GAME_WIDTH:
            continue

        spritelist.add(SPRITE_PIPE, pipe_x, GAME_TOP)

        # Ugly hack to fill in the pipes with 50px tall sprites
        # because we don't have vscaling yet
        for n in (50, 100, 150):
            if top_pipe_height - CAP_HEIGHT > n:
                spritelist.add(SPRITE_PIPE, pipe_x, GAME_TOP + top_pipe_height - n - CAP_HEIGHT)

        # Top pipe cap
        spritelist.add(SPRITE_PIPE_CAP, pipe_x - 3, top_pipe_end - CAP_HEIGHT)

        spritelist.add(SPRITE_PIPE, pipe_x, bottom_pipe_start + CAP_HEIGHT)

        for n in (50, 100, 150):
            if bottom_pipe_height - CAP_HEIGHT > n:
                spritelist.add(SPRITE_PIPE, pipe_x, GAME_BOTTOM - n)

        # Bottom pipe cap
        spritelist.add(SPRITE_PIPE_CAP, pipe_x - 3, bottom_pipe_start)

        if end_index == level_offset + pipe:
            goal_x = pipe * SLICE_WIDTH - (current_x % SLICE_WIDTH)
            goal_x += int(SLICE_WIDTH / 2) - 16
            goal_y = GAME_TOP + int(GAME_HEIGHT / 2) - 16
            spritelist.add(SPRITE_GOAL, int(goal_x), goal_y, force=True)

            collisionlist.add((int(goal_x), int(goal_y), 32, 32), game_win)

        top_pipe_bounds = (pipe_x, GAME_TOP, PIPE_WIDTH, top_pipe_height)
        bottom_pipe_bounds = (pipe_x, bottom_pipe_start, PIPE_WIDTH, bottom_pipe_height)
        score_zone = (pipe_x + PIPE_WIDTH, top_pipe_end, 1, gap_width)

        collisionlist.add(top_pipe_bounds, game_over)
        collisionlist.add(bottom_pipe_bounds, game_over)
        collisionlist.add(score_zone, score_point, offset=level_offset + pipe)

    spritelist.add(BIRBS[int(current_x / 10) % 3], int(birb.x), int(birb.y), force=True)
    spritelist.display()

    # collisionlist.debug(current_x % GAME_WIDTH)
    collisionlist.test(*birb.bounds())
    collisionlist.clear()


def main_game_win(t_current):
    global game_state

    birb.update()
    spritelist.clear()
    spritelist.add(BIRBS[int(t_current / 10) % 3], int(WIDTH / 6), int(birb.y), force=True)
    spritelist.display()

    countdown = (COUNTDOWN_TIME_MS - t_current) / 1000
    countdown = max(0, countdown)

    draw_info(f"YOU WIN! {countdown:.02f}")

    display.update()

    if countdown == 0:
        new_state(GAME_GET_READY)


def main_game_lose(t_current):
    global game_state

    birb.update()
    spritelist.clear()
    spritelist.add(BIRBS[int(t_current / 10) % 3], int(WIDTH / 6), int(birb.y), force=True)
    spritelist.display()

    countdown = (COUNTDOWN_TIME_MS - t_current) / 1000
    countdown = max(0, countdown)

    draw_info(f"YOU LOSE! {countdown:.02f}")

    display.update()

    if countdown == 0:
        new_state(GAME_GET_READY)


def main_game_getready(t_current):
    birb.update()
    spritelist.clear()
    spritelist.add(BIRBS[int(t_current / 10) % 3], int(WIDTH / 6), int(birb.y), force=True)
    spritelist.display()

    countdown = (COUNTDOWN_TIME_MS - t_current) / 1000
    countdown = max(0, countdown)

    draw_info(f"GET READY {countdown:.02f}!")

    display.update()

    if countdown == 0:
        new_level()
        reset_game()


birb = Birb(9)
spritelist = SpriteList()
collisionlist = CollisionList()
end_index = None

prep_game()
new_state(GAME_GET_READY)

while True:
    t_current = (time.ticks_ms() - t_start)

    t_update_start = time.ticks_ms()
    if game_state == GAME_GET_READY:
        main_game_getready(t_current)

    elif game_state == GAME_RUNNING:
        main_game_running(t_current)

    elif game_state == GAME_WIN:
        main_game_win(t_current)

    elif game_state == GAME_LOSE:
        main_game_lose(t_current)

    t_update_end = time.ticks_ms()

    t_update_took = (t_update_end - t_update_start) * 1000

    time.sleep((1.0 / 60) - t_update_took)
    gc.collect()
