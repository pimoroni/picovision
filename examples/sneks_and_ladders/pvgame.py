import struct

TILE_W = 16
TILE_H = 16
TILE_SIZE = (TILE_W, TILE_H)


# A simple wrapper around `load_sprite` to cache decoded PNG images onto the filesystem
def cached_png(display, filename, source=None):
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


# A class for managing sprites
class SpriteList:
    def __init__(self, display, max_sprites=32):
        self._gfx = display
        self.max_sprites = max_sprites
        self.items = []

    def add(self, image, x, y, v_scale=1, force=False):
        if len(self.items) == self.max_sprites:
            if force:
                self.items.pop(0)
            else:
                return
        self.items.append((image, x, y, v_scale))

    def clear(self):
        self.items = []

    def display(self):
        for i in range(self.max_sprites):
            try:
                image, x, y, v_scale = self.items[i]
                self._gfx.display_sprite(i, image, x, y, v_scale=v_scale)
            except IndexError:
                self._gfx.clear_sprite(i)


# A class for managing rectangle to rectangle collisions
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


# A class for managing a basic sprite actor within a tile grid array
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

    def bounds(self):
        return (self.x, self.y, TILE_W, TILE_H)

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
