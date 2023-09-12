import time
import pngdec
from picographics import PicoGraphics, DISPLAY_PICOVISION, PEN_DV_P5 as PEN
import pvgame
import random

t_start = time.ticks_ms()

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

FRAME_SCALE_X = 2
FRAME_SCALE_Y = 1

TILES_X = int(DISPLAY_WIDTH / pvgame.TILE_W)
TILES_Y = int(DISPLAY_HEIGHT / pvgame.TILE_H) - 3  # Two rows for the top status bar, one row for the bottom fire

SPRITE_W = 16
SPRITE_H = 16

FIRE_FRAMES = 5

# Palette Colours - Full Colour
PALETTE_COLOUR = [
    (0xff, 0xff, 0xff),
    (0x2f, 0x2c, 0x1e),
    (0xff, 0x00, 0x29),
    (0x41, 0x3d, 0x2e),
    (0x50, 0x4a, 0x30),
    (0x15, 0x68, 0x06),
    (0x12, 0x75, 0x00),
    (0x7f, 0x68, 0x03),
    (0x1a, 0x88, 0x05),
    (0x00, 0xbf, 0x00),
    (0x8b, 0x8b, 0x8b),
    (0xbf, 0x9c, 0x00),
    (0x06, 0xdc, 0x06),
    (0xdc, 0xb4, 0x06),
    (0xff, 0xff, 0xff),
    (0x3b, 0x32, 0x1d)
]

BLACK = (0x3b, 0x32, 0x1d)
WHITE = (0xcb, 0xc2, 0xad)

# Palette Colours - 1bit
PALETTE_1BIT = [
    BLACK,
    WHITE,  # Platform lower
    WHITE,  # Full Fire
    WHITE,  # Platform middle
    BLACK,  #
    WHITE,  # Ladder right
    BLACK,  #
    BLACK,  # Snake 2 shadow
    WHITE,  # Ladder Left
    WHITE,  # Snake 1 body
    WHITE,  # Snake Fangs
    WHITE,  # Snake 2 Body
    BLACK,
    BLACK,  # Yellow Fire
    BLACK,
    BLACK
]

display = PicoGraphics(DISPLAY_PICOVISION, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, frame_width=DISPLAY_WIDTH * FRAME_SCALE_X, frame_height=DISPLAY_HEIGHT * FRAME_SCALE_Y, pen_type=PEN)
png = pngdec.PNG(display)

display.set_local_palette(0)
display.set_palette(PALETTE_COLOUR)
display.set_local_palette(1)
display.set_palette(PALETTE_1BIT)

BG = len(PALETTE_COLOUR) - 1

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


def draw_level():
    global SNEK_AL, SNEK_AR, SNEK_BL, SNEK_BR, AAAAHL, AAAAHR

    # Load the animation sheet into RAM, we're skipping the first 64 pixels because that's our tilesheet
    animations = pvgame.cached_png(display, "tiles.png", source=(0, 64, 96, 80))

    animation_data_slot = 0

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our green snake
    SNEK_AL = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(0, 0, 16, 80))
    animation_data_slot = SNEK_AL[-1] + 1  # The next slot is the last frame in the sequence + 1
    SNEK_AR = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(16, 0, 16, 80))
    animation_data_slot = SNEK_AR[-1] + 1  # Ditto

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our yellow snake
    SNEK_BL = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(32, 0, 16, 80))
    animation_data_slot = SNEK_BL[-1] + 1  # Ditto
    SNEK_BR = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(48, 0, 16, 80))
    animation_data_slot = SNEK_BR[-1] + 1  # Ditto

    # Take a 16x80 slice of our animation sheet and cut it into 16x16 frames for our player
    AAAAHR = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(64, 0, 16, 80))
    animation_data_slot = AAAAHR[-1] + 1  # Ditto
    AAAAHL = display.load_animation(animation_data_slot, animations, pvgame.TILE_SIZE, source=(80, 0, 16, 80))

    # Everything but the fire
    display.set_scroll_index_for_lines(1, 0, DISPLAY_HEIGHT - pvgame.TILE_H)
    display.set_scroll_index_for_lines(2, 0, DISPLAY_HEIGHT - pvgame.TILE_H)

    # For scrolling the fire
    display.set_scroll_index_for_lines(3, DISPLAY_HEIGHT - pvgame.TILE_H, DISPLAY_HEIGHT)

    # This sequence is repeated horizontally
    fire_tilemap = [0, 2, 1, 4, 3]

    # Repeat the frame sequence, adding one to each frame and wrapping on FIRE_FRAMES
    for x in range(FIRE_FRAMES + 4):
        next_step = [(n + 1) % FIRE_FRAMES for n in fire_tilemap[-FIRE_FRAMES:]]
        fire_tilemap += next_step

    # Add 1 to each tile, since tiles are indexed 1-16 with 0 being no tile
    fire_tilemap_bytes = bytes([n + 1 for n in fire_tilemap])

    # Load our 64x64, 16 tile sheet
    tiles = pvgame.cached_png(display, "tiles.png", source=(0, 0, 64, 64))

    # Clear to background colour
    display.set_pen(BG)
    # Draw a rectangle, since a normal clear would clear the whole 4x wide display
    display.rectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT)
    # And clear the bit that extends behind the animated fire
    display.rectangle(0, DISPLAY_HEIGHT - pvgame.TILE_H, len(fire_tilemap) * pvgame.TILE_H, pvgame.TILE_H)

    # Draw the level data
    display.tilemap(level_data_bytes, (0, 32, 20, 12), tiles)
    display.tilemap(fire_tilemap_bytes, (0, DISPLAY_HEIGHT - pvgame.TILE_H, len(fire_tilemap), 1), tiles)

    display.update()


# Draw the level into both PSRAM buffers
draw_level()


class Player(pvgame.Actor):
    WALKING = 0
    CLIMBING_UP = 1
    CLIMBING_DOWN = 2

    def __init__(self, *args, **kwargs):
        pvgame.Actor.__init__(self, *args, **kwargs)
        self.old_v_x = None
        self.state = self.WALKING
        self.last_ladder = None
        
    def move(self, level_data):
        self.update(level_data)
        
        tile_x = int(self.x / pvgame.TILE_W)
        tile_y = int((self.y + 15) / pvgame.TILE_H)
    
        if self.last_ladder is not None:
            ladder_distance = abs(tile_x - self.last_ladder)
        
            if ladder_distance >= 3:
                self.last_ladder = None

        tile_behind = level_data[tile_y][tile_x]
        try:
            tile_below = level_data[tile_y + 1][tile_x]
        except IndexError:
            tile_below = 0

        if self.state == self.WALKING:
            if tile_x != self.last_ladder:
                if tile_behind == 4 and self.x % pvgame.TILE_W == 0 and random.randint(1, 2) == 1:
                    self.old_v_x = self.v_x
                    self.v_x = 0
                    self.v_y = -1
                    self.state = self.CLIMBING_UP
                    self.last_ladder = tile_x
                    print("Climbing Up")
                elif tile_below == 5 and self.x % pvgame.TILE_W == 0 and random.randint(1, 2) == 1:
                    self.old_v_x = self.v_x
                    self.v_x = 0
                    self.v_y = 1
                    self.state = self.CLIMBING_DOWN
                    self.last_ladder = tile_x
                    print("Climbing Down")
        elif self.state == self.CLIMBING_UP:
            if tile_behind == 0:
                self.y = int(self.y / pvgame.TILE_H) * pvgame.TILE_H
                self.v_y = 0
                self.v_x = self.old_v_x
                self.state = self.WALKING
                print("Walking")
        elif self.state == self.CLIMBING_DOWN:
            if tile_behind in (1, 2, 3):
                self.y = int(self.y / pvgame.TILE_H) * pvgame.TILE_H
                self.v_y = 0
                self.v_x = self.old_v_x
                self.state = self.WALKING
                print("Walking")


spritelist = pvgame.SpriteList(display)
player = Player(spritelist, 5 * pvgame.TILE_H, 2 * pvgame.TILE_H, AAAAHL, AAAAHR, ping_pong=True)
snake_a = pvgame.Actor(spritelist, 2 * pvgame.TILE_H, 2 * pvgame.TILE_H, SNEK_AL, SNEK_AR)
snake_b = pvgame.Actor(spritelist, 4 * pvgame.TILE_H, 6 * pvgame.TILE_H, SNEK_BL, SNEK_BR)
snake_c = pvgame.Actor(spritelist, 2 * pvgame.TILE_H, 8 * pvgame.TILE_H, SNEK_AL, SNEK_AR)
snake_d = pvgame.Actor(spritelist, 16 * pvgame.TILE_H, 8 * pvgame.TILE_H, SNEK_BL, SNEK_BR)

t_end = time.ticks_ms()
print(f"Startup time: {t_end - t_start}ms")

t_start = time.time()

while True:
    pal = int((time.time() - t_start) / 5) % 2
    display.set_remote_palette(pal)

    fire_x = int(time.ticks_ms() / 200) % FIRE_FRAMES
    fire_x *= SPRITE_W * FIRE_FRAMES
    display.set_display_offset(fire_x, 0, 3)

    t = int(time.ticks_ms() / 250)

    player.move(level_data)
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

