import time
import machine
from picographics import PicoGraphics, PEN_RGB555
from picovector import PicoVector, ANTIALIAS_X16

t_start = time.ticks_ms()

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720

display = PicoGraphics(
    PEN_RGB555, DISPLAY_WIDTH, DISPLAY_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT
)
vector = PicoVector(display)
vector.set_antialiasing(ANTIALIAS_X16)

pirateLogo = [
    0b0000001111000000,
    0b0000111111100000,
    0b0111110001110000,
    0b1111110110110000,
    0b1111111001111110,
    0b1111111111111111,
    0b1111001111111111,
    0b0110000001111111,
    0b0010110000011111,
    0b0010000010011110,
    0b0111110000011000,
    0b0111011110111000,
    0b0111000011111000,
    0b0111111111110000,
    0b0011111111100000,
    0b0010111111000000,
    0b0000010011000000,
]
groupOffsets = [(0, 30), (30, 25), (55, 30), (75, 15), (90, 50), (140, 40), (180, 60)]
PALETTE = [
    (0x0, 0x00, 0xFF),
    (0x0, 0x10, 0xFF),
    (0x0, 0x20, 0xFF),
    (0x0, 0x30, 0xFF),
    (0x0, 0x40, 0xFF),
    (0x0, 0x50, 0xFF),
    (0x0, 0x60, 0xFF),
    (0x0, 0x70, 0xFF),
    (0x0, 0x80, 0xFF),
    (0x0, 0x90, 0xFF),
    (0x0, 0xA0, 0xFF),
    (0x0, 0xB0, 0xFF),
    (0x0, 0xC0, 0xFF),
    (0x0, 0xD0, 0xFF),
    (0x0, 0xE0, 0xFF),
    (0x0, 0xF0, 0xFF),
]
PIRATE_PALETTE = [
    (0xF0, 0xF0, 0xF0),
    (0xF0, 0xF0, 0x00),
    (0x00, 0xF0, 0xF0),
    (0x00, 0xF0, 0xF0),
    (0xF0, 0x00, 0xF0),
    (0xF0, 0x00, 0x00),
    (0x00, 0x00, 0xF0),
    (0x00, 0x00, 0x00),
]
button_y = machine.Pin(9, machine.Pin.IN, machine.Pin.PULL_UP)

BLACK = display.create_pen(0, 0, 0)
WHITE = display.create_pen(250, 250, 250)
DARKGREY = display.create_pen(32, 32, 32)
GREY = display.create_pen(96, 96, 96)
SKYBLUE = display.create_pen(32, 32, 240)
SEABLUE = display.create_pen(32, 32, 128)
LIGHTSKYBLUE = display.create_pen(64, 128, 240)
RED = display.create_pen(192, 32, 32)
GREEN = display.create_pen(32, 192, 32)

display.set_font("bitmap8")

modechange = 0


def drawClouds(XPOS, YPOS, SIZE):
    if SIZE == 1:
        display.circle(XPOS + 10, YPOS + 20, 7)
        display.circle(XPOS + 50, YPOS + 20, 7)
        display.circle(XPOS + 20, YPOS + 14, 7)
        display.circle(XPOS + 30, YPOS + 10, 7)
        display.circle(XPOS + 40, YPOS + 16, 7)
        display.rectangle(XPOS + 10, YPOS + 14, 40, 14)

    if SIZE == 2:
        display.circle(XPOS + 10, YPOS + 17, 5)
        display.circle(XPOS + 45, YPOS + 17, 5)
        display.circle(XPOS + 20, YPOS + 10, 6)
        display.circle(XPOS + 29, YPOS + 6, 6)
        display.circle(XPOS + 35, YPOS + 13, 6)
        display.rectangle(XPOS + 12, YPOS + 11, 33, 12)

    if SIZE == 3:
        display.circle(XPOS + 10, YPOS + 12, 4)
        display.circle(XPOS + 40, YPOS + 12, 4)
        display.circle(XPOS + 15, YPOS + 10, 5)
        display.circle(XPOS + 20, YPOS + 5, 5)
        display.circle(XPOS + 35, YPOS + 6, 4)
        display.circle(XPOS + 25, YPOS + 8, 6)
        display.rectangle(XPOS + 12, YPOS + 8, 30, 10)

    if SIZE == 4:
        display.circle(XPOS + 10, YPOS + 8, 4)
        display.circle(XPOS + 35, YPOS + 8, 4)
        display.circle(XPOS + 18, YPOS + 5, 4)
        display.circle(XPOS + 22, YPOS + 5, 3)
        display.circle(XPOS + 28, YPOS + 5, 4)
        display.circle(XPOS + 35, YPOS + 9, 3)
        display.rectangle(XPOS + 10, YPOS + 6, 27, 7)


def drawLogo(s):
    t = int(s / 3) - 160
    x = t + 17
    y = 95
    p = int(s / 5) % 8
    display.set_pen(
        display.create_pen(
            PIRATE_PALETTE[p][0], PIRATE_PALETTE[p][1], PIRATE_PALETTE[p][2]
        )
    )
    for row in range(17):
        for col in range(16):
            if pirateLogo[row] & (1 << col):
                display.pixel(x - col, row + y)


def drawShip(s):
    t = int(s / 3) - 160
    # t = 100
    x = t
    # display.set_pen(LIGHTSKYBLUE)
    # display.rectangle(0,90,DISPLAY_WIDTH,50)
    display.set_pen(display.create_pen(128, 32, 12))
    display.polygon(
        (x + 7, 140),
        (x + 4, 132),
        (x + 1, 132),
        (t, 115),
        (x + 20, 115),
        (x + 22, 120),
        (x + 25, 127),
        (x + 87, 127),
        (x + 92, 122),
        (x + 107, 122),
        (x + 127, 118),
        (x + 107, 127),
        (x + 97, 140),
    )
    display.polygon((x + 1, 120), (x + -3, 93), (x, 93), (x + 1, 120))

    display.rectangle(x + 30, 90, 2, 40)
    display.rectangle(x + 55, 90, 2, 40)
    display.rectangle(x + 80, 100, 2, 30)
    display.set_pen(BLACK)
    display.polygon(
        (x + 1, 112),
        (x - 1, 93),
        (x + 19 + (int(s / 4) % 4), 93),
        (x + 19, 102),
        (x + 20 + (int(s / 4) % 3), 112),
    )

    display.set_pen(display.create_pen(220, 220, 220))
    display.rectangle(x + 56, 90, 2 + int(s / 5) % 4, 2)
    x = t - 15
    y = -3
    display.polygon(
        (x + 30, 95 + y),
        (x + 55, 95 + y),
        (x + 58, 100 + y),
        (x + 59, 104 + y),
        (x + 57, 106 + y),
        (x + 50, 110 + y),
        (x + 50, 109 + y),
        (x + 53, 106 + y),
        (x + 35, 115 + y),
        (x + 45, 105 + y),
        (x + 45, 100 + y),
        (x + 40, 98 + y),
    )
    y = 14
    display.polygon(
        (x + 30, 95 + y),
        (x + 55, 95 + y),
        (x + 58, 100 + y),
        (x + 59, 104 + y),
        (x + 57, 106 + y),
        (x + 50, 110 + y),
        (x + 50, 109 + y),
        (x + 53, 106 + y),
        (x + 35, 115 + y),
        (x + 45, 105 + y),
        (x + 45, 100 + y),
        (x + 40, 98 + y),
    )
    x = t + 10
    y = -2
    display.polygon(
        (x + 30, 95 + y),
        (x + 55, 95 + y),
        (x + 58, 100 + y),
        (x + 59, 104 + y),
        (x + 57, 106 + y),
        (x + 50, 110 + y),
        (x + 50, 109 + y),
        (x + 53, 106 + y),
        (x + 35, 115 + y),
        (x + 45, 105 + y),
        (x + 45, 100 + y),
        (x + 40, 98 + y),
    )
    y = 15
    display.polygon(
        (x + 30, 95 + y),
        (x + 55, 95 + y),
        (x + 58, 100 + y),
        (x + 59, 104 + y),
        (x + 57, 106 + y),
        (x + 50, 110 + y),
        (x + 50, 109 + y),
        (x + 53, 106 + y),
        (x + 35, 115 + y),
        (x + 45, 105 + y),
        (x + 45, 100 + y),
        (x + 40, 98 + y),
    )
    y = 9
    x = t + 30
    display.polygon(
        (x + 30, 95 + y),
        (x + 55, 95 + y),
        (x + 58, 100 + y),
        (x + 59, 104 + y),
        (x + 57, 106 + y),
        (x + 50, 110 + y),
        (x + 50, 109 + y),
        (x + 53, 106 + y),
        (x + 35, 115 + y),
        (x + 45, 105 + y),
        (x + 45, 100 + y),
        (x + 40, 98 + y),
    )
    x = t + 78
    y = 0
    display.polygon(
        (x, 98),
        (x + 15, 102),
        (x + 18, 102),
        (x + 24, 107),
        (x + 27, 112),
        (x + 30, 117),
        (x + 10, 122),
        (x + 10, 121),
        (x + 15, 114),
        (x + 14, 112),
        (x + 12, 110),
        (x + 10, 105),
    )


while True:
    for p in range(2):
        display.set_scroll_group_for_lines(1, 0, 30)
        display.set_scroll_group_for_lines(2, 30, 55)
        display.set_scroll_group_for_lines(3, 55, 75)
        display.set_scroll_group_for_lines(4, 75, 90)
        display.set_scroll_group_for_lines(5, 140, 158)
        display.set_scroll_group_for_lines(6, 158, 190)
        display.set_scroll_group_for_lines(7, 190, 240)

        display.set_pen(LIGHTSKYBLUE)
        display.clear()
        display.set_pen(SKYBLUE)
        display.rectangle(0, 0, FRAME_WIDTH, 30)
        display.set_pen(display.create_pen(40, 56, 240))
        display.rectangle(0, 30, FRAME_WIDTH, 25)
        display.set_pen(display.create_pen(48, 80, 240))
        display.rectangle(0, 55, FRAME_WIDTH, 20)
        display.set_pen(display.create_pen(58, 112, 240))
        display.rectangle(0, 75, FRAME_WIDTH, 15)
        display.rectangle(0, 140, FRAME_WIDTH, 40)
        display.set_pen(SEABLUE)
        display.rectangle(0, 180, FRAME_WIDTH, 60)
        display.set_pen(WHITE)

        for f in range(20):
            drawClouds(f * 100, 1, 1)
        for f in range(23):
            drawClouds(f * 90, 31, 2)
        for f in range(28):
            drawClouds(f * 80, 56, 3)
        for f in range(33):
            drawClouds(f * 70, 76, 4)

        for s in range(200):
            for f in range(60):
                display.set_pen(
                    display.create_pen(
                        PALETTE[s % 8][0],
                        PALETTE[s % 12 + 2][1],
                        PALETTE[s % 12 + 2][2],
                    )
                )
                display.circle(f * 30, 720 - s, 20)
                display.set_pen(
                    display.create_pen(
                        PALETTE[s % 8][0], PALETTE[s % 8 + 5][1], PALETTE[s % 8 + 5][2]
                    )
                )
                display.circle(f * 20, 570 - s, 12)
                display.set_pen(
                    display.create_pen(
                        PALETTE[s % 8][0], PALETTE[s % 6 + 6][1], PALETTE[s % 6 + 6][2]
                    )
                )
                display.circle(f * 15, 420 - s, 12)

        display.set_pen(LIGHTSKYBLUE)
        display.rectangle(0, 90, FRAME_WIDTH, 50)
        display.set_pen(SEABLUE)
        display.text("Pimoroni - Scroll Groups - at Sea", 10, 95, 300, 1)

        display.set_scroll_group_offset(5, 200, 150)
        display.set_scroll_group_offset(6, 300, 250)
        display.set_scroll_group_offset(7, 400, 350)

        display.update()

    while True:
        for s in range(1800):
            display.set_scroll_group_offset(1, int(s / 2), 0)
            display.set_scroll_group_offset(2, int(s / 2.5), 0)
            display.set_scroll_group_offset(3, int(s / 4.5), 0)
            display.set_scroll_group_offset(4, int(s / 6.4), 0)

            a = 1900 - s
            w = int(s % 60) - 30
            w = abs(w)
            # print(w)
            display.set_scroll_group_offset(5, int(w / 4), int(a / 20) % 24 + 150)
            display.set_scroll_group_offset(6, 20 - int(w / 3), int(a / 15) % 24 + 250)
            display.set_scroll_group_offset(7, int(w / 2), int(a / 10) % 24 + 350)
            # time.sleep(0.01)

            display.set_pen(LIGHTSKYBLUE)
            display.rectangle(0, 90, DISPLAY_WIDTH, 50)

            drawShip(s)
            drawLogo(s)
            # drawShip(620)
            # drawLogo(620)

            display.update()

        # display.set_scroll_group_offset(1, X, Y)
