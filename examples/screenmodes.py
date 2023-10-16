import gc
import time
import machine
from picographics import PicoGraphics, PEN_RGB555, WIDESCREEN

DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 240

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720

# number of modes listed in array
numModes = 16
# current mode being dispalyed
currentMode = 0
# mode which will be selected with the Y button
modeSelect = 0
modes = [
    (320, 240, 0),
    (360, 240, 0),
    (360, 288, 0),
    (400, 225, 1),
    (400, 240, 1),
    (400, 300, 1),
    (480, 270, 1),
    (960, 270, 1),
    (640, 480, 0),
    (720, 480, 0),
    (720, 576, 0),
    (800, 450, 1),
    (800, 480, 1),
    (800, 600, 1),
    (960, 540, 1),
    (1280, 720, 1),
]

display = PicoGraphics(
    PEN_RGB555, DISPLAY_WIDTH, DISPLAY_HEIGHT, FRAME_WIDTH, FRAME_HEIGHT
)

modeChange = 0
button_y = machine.Pin(9, machine.Pin.IN, machine.Pin.PULL_UP)

BLACK = display.create_pen(0, 0, 0)
WHITE = display.create_pen(250, 250, 250)
DARKGREY = display.create_pen(32, 32, 32)
GREY = display.create_pen(96, 96, 96)
SKYBLUE = display.create_pen(32, 32, 192)
RED = display.create_pen(192, 32, 32)
GREEN = display.create_pen(32, 192, 32)

display.set_font("bitmap8")

modechange = 0

while True:
    for p in range(2):
        if modeChange == 0:
            display.set_pen(0)
            display.clear()

        for f in range(720):
            display.set_pen(display.create_pen(0, 0, int(f / 3)))
            display.line(0, f, FRAME_WIDTH, f)

        display.set_pen(WHITE)
        display.text("Pimoroni Screen Mode Selector", 0, 0, 300, 1)

        if modeChange == 0 or modeChange > 2:
            display.update()
    if modeChange > 4:
        modeChange = 0

    elif modeChange > 2:
        modeChange -= 2

    while True:
        if modeChange > 2:
            break
        if display.is_button_a_pressed():
            print("X is pressed")
            modeSelect -= 1
            if modeSelect < 0:
                modeSelect = numModes - 1
            while display.is_button_a_pressed():
                m = 0
        if display.is_button_x_pressed():
            print("A is pressed")
            modeSelect += 1
            if modeSelect > numModes - 1:
                modeSelect = 0
            while display.is_button_x_pressed():
                m = 0

        for p in range(2):
            display.set_pen(DARKGREY)
            display.rectangle(14, 22, 137, numModes * 10 + 14)
            for f in range(numModes):
                display.set_pen(GREY)
                if f == modeSelect:
                    display.set_pen(SKYBLUE)
                    if button_y.value() == 0:
                        if modeSelect != currentMode:
                            if modes[f][2] and WIDESCREEN == 0:
                                display.set_pen(RED)
                                modeChange = -1
                            else:
                                modeChange = 3
                                display.set_pen(GREEN)

                display.rectangle(20, f * 10 + 29, 125, 10)
                display.set_pen(WHITE)
                modeText = "{width:} x {height:}".format(
                    width=modes[f][0], height=modes[f][1]
                )
                display.text(modeText, 30, f * 10 + 30, 300, 1)

                if currentMode == f:
                    display.text(">", 22, f * 10 + 30, 300, 1)
                    display.text("<", 137, f * 10 + 30, 300, 1)

            display.update()

        if modeChange == 2:
            for f in range(100):
                display.set_pen(GREEN)
                display.rectangle(20, modeSelect * 10 + 29, 125, 10)
                display.set_pen(WHITE)
                display.text(
                    "OK - Hit Y {timeout:}".format(timeout=int((99 - f) / 10)),
                    30,
                    modeSelect * 10 + 30,
                    300,
                    1,
                )
                display.update()
                time.sleep(0.15)

                if button_y.value() == 0:
                    break
                    modechange = 0
            if f > 98:
                del display
                gc.collect()
                display = PicoGraphics(
                    PEN_RGB555,
                    modes[currentMode][0],
                    modes[currentMode][1],
                    FRAME_WIDTH,
                    FRAME_HEIGHT,
                )
            else:
                currentMode = modeSelect
            modeChange = 5

        if modeChange == -1:
            display.set_pen(RED)
            display.rectangle(20, modeSelect * 10 + 29, 125, 10)
            display.set_pen(WHITE)
            display.text("Unsupported - Wide Only", 30, modeSelect * 10 + 30, 300, 1)
            display.update()
            modeChange = 0
            time.sleep(0.25)

        if modeChange == 1:
            display.set_pen(GREEN)
            display.rectangle(20, modeSelect * 10 + 29, 125, 10)
            display.set_pen(WHITE)
            display.text("Mode Change", 30, modeSelect * 10 + 30, 300, 1)
            display.update()
            time.sleep(0.9)

            del display
            gc.collect()
            display = PicoGraphics(
                PEN_RGB555,
                modes[modeSelect][0],
                modes[modeSelect][1],
                FRAME_WIDTH,
                FRAME_HEIGHT,
            )

            modeChange = 4
