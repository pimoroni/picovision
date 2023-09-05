import time
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

display = PicoGraphics(DISPLAY_PICOVISION, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, frame_width=DISPLAY_WIDTH * FRAME_SCALE_X, frame_height=DISPLAY_HEIGHT * FRAME_SCALE_Y, pen_type=PEN)
png = pngdec.PNG(display)


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


class TileData:
    def __init__(self, filename, source=None, cache=None):
        self._filename = filename
        self._cache = cache
        try:
            x, y, w, h = source
        except ValueError:
            raise RuntimeError("TileData requires a source region.")
        try:
            self._data = open(self._cache, "rb").read()
            self._width = w
            self._height = h
        except OSError:
            self._width, self._height, self._data = display.load_sprite(filename, source=source)
            if cache is not None:
                open(self._cache, "wb").write(self._data)

    def purge(self):
        pass

    def data(self):
        return self._width, self._height, self._data


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


# Load the snake sprite data into the PSRAM
snek_data = TileData("tiles.png", cache="snek_data.bin", source=(0, 64, 32, 80))
print(snek_data.data())
start_frame = 0
end_frame = display.load_animation(start_frame, snek_data.data(), (16, 16))

SNEK_A = list(range(start_frame, end_frame))[0::2]
SNEK_B = list(range(start_frame, end_frame))[1::2]


aaah_data = TileData("tiles.png", cache="aaah_data.bin", source=(48, 64, 16, 80))
start_frame = end_frame
end_frame = display.load_animation(start_frame, aaah_data.data(), (16, 16))

AAAAAH = list(range(start_frame, end_frame))

spritelist = SpriteList()


for _ in range(2):
    display.set_scroll_index_for_lines(1, 0, DISPLAY_HEIGHT - 16)
    display.set_scroll_index_for_lines(2, 0, DISPLAY_HEIGHT - 16)

    # For scrolling the fire
    display.set_scroll_index_for_lines(3, DISPLAY_HEIGHT - 16, DISPLAY_HEIGHT)

    display.update()


NUM_FRAMES = 5

# This sequence is repeated horizontally
fire_tilemap = [0, 2, 1, 4, 3]

# Repeat the frame sequence, adding one to each frame and wrappong on NUM_FRAMES
for x in range(NUM_FRAMES + 4):
    next_step = [(n + 1) % NUM_FRAMES for n in fire_tilemap[-NUM_FRAMES:]]
    fire_tilemap += next_step

# Add 1 to each tile, since tiles are indexed 1-16 with 0 being no tile
fire_tilemap_bytes = bytes([n + 1 for n in fire_tilemap])

# Load our 64x64, 16 tile sheet
tiles = TileData("tiles.png", cache="tiles.bin", source=(0, 0, 64, 64))

# Clear to background colour
display.set_pen(BG)
# Draw a rectangle, since a normal clear would clear the whole 4x wide display
display.rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT)
# And clear the bit that extends behind the animated fire
display.rectangle(0, DISPLAY_HEIGHT - 16, len(fire_tilemap) * 16, 16)

# Draw the level data
display.tilemap(level_data_bytes, (0, 32, 20, 12), tiles.data())
display.tilemap(fire_tilemap_bytes, (0, DISPLAY_HEIGHT - 16, len(fire_tilemap), 1), tiles.data())
display.update()


t_end = time.ticks_ms()
print(f"Startup time: {t_end - t_start}ms")


while True:
    fire_x = int(time.ticks_ms() / 200) % NUM_FRAMES
    fire_x *= SPRITE_W * NUM_FRAMES
    display.set_display_offset(fire_x, 0, 3)

    snek_frame = int(time.ticks_ms() / 250) % len(SNEK_A)

    spritelist.clear()
    spritelist.add(SNEK_A[snek_frame], 50 + 0, 64)
    spritelist.add(SNEK_B[snek_frame], 50 + 16, 64)
    spritelist.add(SNEK_A[snek_frame], 50 + 32, 64)
    spritelist.add(SNEK_B[snek_frame], 50 + 48, 64)
    spritelist.add(AAAAAH[snek_frame], 110, 64)
    spritelist.display()
