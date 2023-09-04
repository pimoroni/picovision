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

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

FRAME_SCALE_X = 4
FRAME_SCALE_Y = 1

TILE_W = 16
TILE_H = 16

TILES_X = int(DISPLAY_WIDTH / TILE_W)
TILES_Y = int(DISPLAY_HEIGHT / TILE_H) - 3  # Two rows for the top status bar, one row for the bottom fire

SPRITE_W = 16
SPRITE_H = 16

display = PicoGraphics(DISPLAY_PICOVISION, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, frame_width=DISPLAY_WIDTH * FRAME_SCALE_X, frame_height=DISPLAY_HEIGHT * FRAME_SCALE_Y, pen_type=PEN_DV_RGB555)
png = pngdec.PNG(display)


level_data = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1],
]


class SpriteData:
    MAX_SPRITE_DATA_SLOTS = 32

    def __init__(self):
        self.files = [None for _ in range(self.MAX_SPRITE_DATA_SLOTS)]
        self.crops = [None for _ in range(self.MAX_SPRITE_DATA_SLOTS)]
        
    def add(self, filename, crop):
        slot = self.files.index(None)
        self.files[slot] = filename
        self.crops[slot] = crop
        return slot
        
    def load(self):
        # Load sprites into each PSRAM buffer
        for _ in range(2):
            for index, filename in enumerate(self.files):
                if filename is not None:
                    crop = self.crops[index]
                    if crop is not None:
                        display.load_sprite(filename, index, crop)
                    else:
                        display.load_sprite(filename, index)
            display.update()

    def split(self, filename, bounds, sprite_size):
        bx, by, bw, bh = bounds
        sw, sh = sprite_size

        sprites_x = int(bw / sw)
        sprites_y = int(bh / sh)

        indexes = []

        for y in range(sprites_y):
            for x in range(sprites_x):
                i = self.add(filename, (bx + (x * sw), by + (y * sh), sw, sh))
                indexes.append(i)

        return indexes


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


spritedata = SpriteData()

FIRE = spritedata.split("fire.png", (0, 0, 16, 80), (16, 16))

SNEK_A = spritedata.split("snek.png", (0, 0, 16, 80), (16, 16))
SNEK_B = spritedata.split("snek.png", (16, 0, 16, 80), (16, 16))

spritedata.load()

spritelist = SpriteList()


for _ in range(2):
    display.set_scroll_index_for_lines(1, 0, DISPLAY_HEIGHT - 16)
    display.set_scroll_index_for_lines(2, 0, DISPLAY_HEIGHT - 16)

    # For scrolling the fire
    display.set_scroll_index_for_lines(3, DISPLAY_HEIGHT - 16, DISPLAY_HEIGHT)

    display.update()


NUM_FRAMES = 5

# This sequence is repeated horizontally
start_frame = [0, 2, 1, 4, 3]

print("Drawing Level")

png_data = open("fire.png", "rb").read()
png.open_RAM(png_data)

for _ in range(2):
    display.set_pen(display.create_pen(0x3b, 0x32, 0x1d))
    display.clear()
    display.set_pen(display.create_pen(255, 255, 255))
    o_r = 0
    for f in range(NUM_FRAMES + 4):  # A little bit of right padding to repeat the pattern
        for r in range(NUM_FRAMES):
            frame = (start_frame[r % NUM_FRAMES] + f) % NUM_FRAMES
            png.decode(o_r, DISPLAY_HEIGHT - SPRITE_H, source=(0, frame * SPRITE_H, SPRITE_W, SPRITE_H))
            #display.text(f"{frame}", o_r, DISPLAY_HEIGHT - 32)
            o_r += SPRITE_W
    display.update()

png_data = open("platform.png", "rb").read()
png.open_RAM(png_data)

for _ in range(2):
    for y in range(TILES_Y):
        ry = (y + 2) * TILE_H
        for x in range(TILES_X):
            rx = x * TILE_W
            if level_data[y][x]:
                #display.rectangle(rx, ry, TILE_W, int(TILE_H / 4))
                tile_type = 1  # middle platform section
                if x > 0 and level_data[y][x - 1] == 0:
                    tile_type = 0  # left platform section
                elif x < TILES_X - 1 and level_data[y][x + 1] == 0:
                    tile_type = 2  # Right platform section

                png.decode(rx, ry, source=(tile_type * TILE_W, 0, TILE_W, TILE_H))

    display.update()
print("Done")


while True:
    fire_x = int(time.ticks_ms() / 200) % NUM_FRAMES
    fire_x *= SPRITE_W * NUM_FRAMES
    display.set_display_offset(fire_x, 0, 3)

    snek_frame = int(time.ticks_ms() / 250) % len(SNEK_A)

    spritelist.clear()
    spritelist.add(SNEK_A[snek_frame], 0, 64)
    spritelist.add(SNEK_B[snek_frame], 16, 64)
    spritelist.add(SNEK_A[snek_frame], 32, 64)
    spritelist.add(SNEK_B[snek_frame], 48, 64)
    spritelist.display()
