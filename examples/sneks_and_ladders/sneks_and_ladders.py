import time
import struct
import pngdec
from picographics import PicoGraphics, DISPLAY_PICOVISION, PEN_DV_P5 as PEN

t_start = time.ticks_ms()

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

FRAME_SCALE_X = 2
FRAME_SCALE_Y = 1

TILE_W = 16
TILE_H = 16

TILES_X = int(DISPLAY_WIDTH / TILE_W)
TILES_Y = int(DISPLAY_HEIGHT / TILE_H) - 3  # Two rows for the top status bar, one row for the bottom fire

SPRITE_W = 16
SPRITE_H = 16

FIRE_FRAMES = 5

display = PicoGraphics(DISPLAY_PICOVISION, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, frame_width=DISPLAY_WIDTH * FRAME_SCALE_X, frame_height=DISPLAY_HEIGHT * FRAME_SCALE_Y, pen_type=PEN)
png = pngdec.PNG(display)


# Palette Colours - Full Colour
display.create_pen(0xff, 0xff, 0xff)
display.create_pen(0x2f, 0x2c, 0x1e)
display.create_pen(0xff, 0x00, 0x29)
display.create_pen(0x41, 0x3d, 0x2e)
display.create_pen(0x50, 0x4a, 0x30)
display.create_pen(0x15, 0x68, 0x06)
display.create_pen(0x12, 0x75, 0x00)
display.create_pen(0x7f, 0x68, 0x03)
display.create_pen(0x1a, 0x88, 0x05)
display.create_pen(0x00, 0xbf, 0x00)
display.create_pen(0x8b, 0x8b, 0x8b)
display.create_pen(0xbf, 0x9c, 0x00)
display.create_pen(0x06, 0xdc, 0x06)
display.create_pen(0xdc, 0xb4, 0x06)
display.create_pen(0xff, 0xff, 0xff)

BG = display.create_pen(0x3b, 0x32, 0x1d)

"""
# Palette Colours - 1bit mode
display.create_pen(0x00, 0x00, 0x00)
display.create_pen(0xff, 0xff, 0xff)  #   Platform lower
display.create_pen(0x00, 0x00, 0x00)
display.create_pen(0xff, 0xff, 0xff)  #   Platform middle
display.create_pen(0x00, 0x00, 0x00)  #
display.create_pen(0xff, 0xff, 0xff)  #   Ladder right
display.create_pen(0x00, 0x00, 0x00)  #
display.create_pen(0xff, 0xff, 0xff)  #   Snake 2 shadow
display.create_pen(0xff, 0xff, 0xff)  #   Ladder Left
display.create_pen(0xff, 0xff, 0xff)  #   Snake 1 body
display.create_pen(0xff, 0xff, 0xff)  #   Snake Fangs
display.create_pen(0xff, 0xff, 0xff)  #   Snake 2 Body
display.create_pen(0x00, 0x00, 0x00)
display.create_pen(0xff, 0xff, 0xff)

BG = display.create_pen(0x00, 0x00, 0x00)
"""

level_data = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 1, 2, 2, 2, 2, 2, 5, 2, 2, 2, 2, 2, 2, 2, 5, 2, 3, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0],
    [0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 5, 2, 2, 2, 2, 5, 3, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 1, 5, 2, 5, 2, 2, 3, 0, 0, 0, 4, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0],
    [0, 0, 0, 0, 1, 5, 2, 2, 3, 0, 4, 0, 1, 2, 2, 3, 0, 4, 0, 0],
    [0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0],
    [0, 1, 2, 5, 2, 3, 0, 0, 1, 2, 2, 2, 5, 3, 0, 1, 5, 2, 2, 2],
    [0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0],
    [2, 2, 2, 2, 2, 3, 0, 0, 1, 2, 2, 2, 2, 3, 0, 1, 2, 2, 2, 2],
]

# Crush the 2d list above into a simple bytearray
level_data_bytes = b""

for row in level_data:
    # Add 5 to each index, to move us past the fire tiles to the platforms
    level_data_bytes += bytes([n + 5 if n > 0 else n for n in row])


# A basic wrapper around loading tile data from PNG and caching it to .bin
def cached_png(filename, source=None):
    cache = filename[:-4]  # strip .png
    if source is not None:
        x, y, w, h = source
        cache = f"{cache}-{x}-{y}-{w}-{h}"
    cache += ".bin"

    try:
        with open(cache, "rb") as f:
            width, height = struct.unpack("HH", f.read(4))
            data = f.read()
    except OSError:
        width, height, data = display.load_sprite(filename, source=source)

        with open(cache, "wb") as f:
            f.write(struct.pack("HH", width, height))
            f.write(data)
    return width, height, data


class SpriteList:
    def __init__(self):
        self.items = []

    def add(self, image, x, y, v_scale=1, force=False):
        if len(self.items) == 32:
            if force:
                self.items.pop(0)
            else:
                return
        self.items.append((image, x, y, v_scale))

    def clear(self):
        self.items = []

    def display(self):
        for i in range(32):
            try:
                image, x, y, v_scale = self.items[i]
                display.display_sprite(i, image, x, y, v_scale=v_scale)
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


class Actor:
    LEFT = True
    RIGHT = False

    def __init__(self, spritelist, x, y, frames_left, frames_right, ping_pong=False):
        self.spritelist = spritelist
        self.x = x
        self.y = y
        self.v_x = -1
        self.v_y = 0
        self.frames_left = frames_left
        self.frames_right = frames_right

        if ping_pong:
            self.frames_left += reversed(frames_left[1:][:-1])
            self.frames_right += reversed(frames_right[1:][:-1])

        self.l_count = len(self.frames_left)
        self.r_count = len(self.frames_right)

        self.facing = self.LEFT

    def update(self, level_data):
        level_width = len(level_data[0]) * TILE_W

        self.x += self.v_x
        self.y += self.v_y

        tile_x = int((self.x + 8) / TILE_W)
        tile_y = int(self.y / TILE_H)

        tile_below = level_data[tile_y + 1][tile_x]

        if tile_below == 0 or self.x < 0 or (self.x + TILE_W) > level_width:
            self.v_x *= -1
            self.x += self.v_x
            self.facing = self.LEFT if self.v_x < 0 else self.RIGHT

    def draw(self, t, offset_x, offset_y):
        if self.facing == self.LEFT:
            frame = self.frames_left[t % self.l_count]
        else:
            frame = self.frames_right[t % self.r_count]
        self.spritelist.add(frame, self.x + offset_x, self.y + offset_y)


def draw_level():
    global SNEK_AL, SNEK_AR, SNEK_BL, SNEK_BR, AAAAHL, AAAAHR

    # Load the animation sheet into RAM, we're skipping the first 64 pixels because that's our tilesheet
    animations = cached_png("tiles.png", source=(0, 64, 96, 80))

    animation_data_slot = 0

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our green snake
    SNEK_AL = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(0, 0, 16, 80))
    animation_data_slot = SNEK_AL[-1] + 1  # The next slot is the last frame in the sequence + 1
    SNEK_AR = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(16, 0, 16, 80))
    animation_data_slot = SNEK_AR[-1] + 1  # Ditto

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our yellow snake
    SNEK_BL = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(32, 0, 16, 80))
    animation_data_slot = SNEK_BL[-1] + 1  # Ditto
    SNEK_BR = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(48, 0, 16, 80))
    animation_data_slot = SNEK_BR[-1] + 1  # Ditto

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our player
    AAAAHR = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(64, 0, 16, 80))
    animation_data_slot = AAAAHR[-1] + 1  # Ditto
    AAAAHL = display.load_animation(animation_data_slot, animations, (TILE_W, TILE_H), source=(80, 0, 16, 80))

    # Everything but the fire
    display.set_scroll_index_for_lines(1, 0, DISPLAY_HEIGHT - TILE_H)
    display.set_scroll_index_for_lines(2, 0, DISPLAY_HEIGHT - TILE_H)

    # For scrolling the fire
    display.set_scroll_index_for_lines(3, DISPLAY_HEIGHT - TILE_H, DISPLAY_HEIGHT)

    # This sequence is repeated horizontally
    fire_tilemap = [0, 2, 1, 4, 3]

    # Repeat the frame sequence, adding one to each frame and wrapping on FIRE_FRAMES
    for x in range(FIRE_FRAMES + 4):
        next_step = [(n + 1) % FIRE_FRAMES for n in fire_tilemap[-FIRE_FRAMES:]]
        fire_tilemap += next_step

    # Add 1 to each tile, since tiles are indexed 1-16 with 0 being no tile
    fire_tilemap_bytes = bytes([n + 1 for n in fire_tilemap])

    # Load our 64x64, 16 tile sheet
    tiles = cached_png("tiles.png", source=(0, 0, 64, 64))

    # Clear to background colour
    display.set_pen(BG)
    # Draw a rectangle, since a normal clear would clear the whole 4x wide display
    display.rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT)
    # And clear the bit that extends behind the animated fire
    display.rectangle(0, DISPLAY_HEIGHT - TILE_H, len(fire_tilemap) * TILE_W, TILE_H)

    # Draw the level data
    display.tilemap(level_data_bytes, (0, 32, 20, 12), tiles)
    display.tilemap(fire_tilemap_bytes, (0, DISPLAY_HEIGHT - TILE_H, len(fire_tilemap), 1), tiles)

    display.update()


draw_level()

spritelist = SpriteList()
player = Actor(spritelist, 5 * TILE_W, 2 * TILE_H, AAAAHL, AAAAHR, ping_pong=True)
snake_a = Actor(spritelist, 2 * TILE_W, 2 * TILE_H, SNEK_AL, SNEK_AR)
snake_b = Actor(spritelist, 4 * TILE_W, 6 * TILE_H, SNEK_BL, SNEK_BR)
snake_c = Actor(spritelist, 2 * TILE_W, 8 * TILE_H, SNEK_AL, SNEK_AR)
snake_d = Actor(spritelist, 16 * TILE_W, 8 * TILE_H, SNEK_BL, SNEK_BR)

t_end = time.ticks_ms()
print(f"Startup time: {t_end - t_start}ms")


while True:
    fire_x = int(time.ticks_ms() / 200) % FIRE_FRAMES
    fire_x *= SPRITE_W * FIRE_FRAMES
    display.set_display_offset(fire_x, 0, 3)

    t = int(time.ticks_ms() / 250)

    player.update(level_data)
    snake_a.update(level_data)
    snake_b.update(level_data)
    snake_c.update(level_data)
    snake_d.update(level_data)

    spritelist.clear()
    player.draw(t, 0, 32)
    snake_a.draw(t, 0, 32)
    snake_b.draw(t, 0, 32)
    snake_c.draw(t, 0, 32)
    snake_d.draw(t, 0, 32)
    spritelist.display()

    time.sleep(1.0 / 30)
