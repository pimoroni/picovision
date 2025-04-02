# PicoVision boot menu/loader.

import gc
import time
import math
import random
from os import listdir, stat
from picovision import PicoVision, PEN_RGB555
from pimoroni import Button

WIDTH = 320
HEIGHT = 240

# Hires (Slow!)
# WIDTH = 640
# HEIGHT = 480


def get_applications() -> list[dict[str, str, str]]:
    # fetch a list of the applications that are stored in the filesystem
    applications = []

    for file in listdir():
        if file.endswith(".py") and file not in ("main.py", "secrets.py"):
            # print(f"App: {file}")
            # convert the filename from "something_or_other.py" to "Something Or Other"
            # via weird incantations and a sprinkling of voodoo
            title = " ".join([v[:1].upper() + v[1:] for v in file[:-3].split("_")])

            applications.append(
                {
                    "file": file,
                    "title": title
                }
            )
        else:
            try:
                stat(f"{file}/{file}.py")
                # convert the filename from "something_or_other.py" to "Something Or Other"
                # via weird incantations and a sprinkling of voodoo
                title = " ".join([v[:1].upper() + v[1:] for v in file.split("_")])

                applications.append(
                    {
                        "file": f"{file}.{file}",
                        "title": title
                    }
                )
            except OSError:
                pass

    # sort the application list alphabetically by title and return the list
    return sorted(applications, key=lambda x: x["title"])


def prepare_for_launch() -> None:
    for k in locals().keys():
        if k not in ("__name__",
                     "application_file_to_launch",
                     "gc"):
            del locals()[k]
    gc.collect()


def hint(display: PicoVision, hint_x: int, hint_y: int, t: float, text: str, fg: int, bg: int) -> None:
    for i in range(len(text)):
        label_y = hint_y + int((math.sin(t * 8 + i) + 1) * 2)
        display.set_pen(bg)
        display.text(text[i], hint_x + 2, label_y, -1, 1)
        display.set_pen(fg)
        display.text(text[i], hint_x + 1, label_y - 1, -1, 1)
        hint_x += display.measure_text(text[i], 1)


class Toaster:
    def __init__(self, frames, slot=0):
        self.w = WIDTH
        self.h = HEIGHT
        self.frames = frames
        self.slot = slot
        self.vx = random.randint(0, 5) - 2.5
        self.vy = random.randint(0, 5) - 2.5
        self.x = random.randint(0, self.w - 32)
        self.y = random.randint(0, self.h - 32)

    def update(self):
        self.x += self.vx
        self.y += self.vy

        if self.x < 0:
            self.x = 0
            self.vx *= -1
        if self.x + 32 > self.w:
            self.x = self.w - 32
            self.vx *= -1
        if self.y < 0:
            self.y = 0
            self.vy *= -1
        if self.y + 32 > self.h:
            self.y = self.h - 32
            self.vy *= -1

    def draw(self, display, t):
        display.display_sprite(self.slot, self.frames[int(t * 16) % len(self.frames)], int(self.x), int(self.y))


def menu() -> str:
    applications = get_applications()
    display = PicoVision(PEN_RGB555, WIDTH, HEIGHT)

    button_up = display.is_button_a_pressed
    button_down = display.is_button_x_pressed
    button_select = Button(9, invert=True).read

    selected_item = 2
    scroll_position = 2
    target_scroll_position = 2
    last_up = 0
    last_down = 0

    selected_pen = display.create_pen(120, 240, 150)
    selected_scanline_pen = display.create_pen(130, 250, 160)
    unselected_pen = display.create_pen(24, 48, 28)
    unselected_scanline_pen = display.create_pen(28, 56, 32)
    background_pen = display.create_pen(11, 17, 14)
    scanline_pen = display.create_pen(5, 10, 6)
    shadow_pen = display.create_pen(12, 24, 14)

    press_x_label = True
    press_a_label = False
    press_y_label = False
    omg_toasters = False
    last_button = time.ticks_ms() / 1000.0

    # Twice to fill the front/back buffers
    for _ in range(2):
        display.set_scroll_group_for_lines(1, HEIGHT - 20, HEIGHT)
        display.set_scroll_group_offset(1, 0, 0, wrap_x=WIDTH)
        try:
            toaster_frames = display.load_animation(0, "toaster.png", (32, 32))
            toaster_frames += list(reversed(toaster_frames[:-1]))
        except OSError:
            toaster_frames = None
        display.update()

    toasters = [
        Toaster(toaster_frames, 0),
        Toaster(toaster_frames, 1),
        Toaster(toaster_frames, 2),
        Toaster(toaster_frames, 3),
    ]

    while True:
        t = time.ticks_ms() / 1000.0
        display.set_scroll_group_offset(1, int(t * 20) % WIDTH, 0, wrap_x=WIDTH, wrap_x_to=0)

        if omg_toasters:
            for toaster in toasters:
                toaster.update()
                toaster.draw(display, t)
        else:
            for i in range(len(toasters)):
                display.clear_sprite(i)

        display.set_font("bitmap8")

        cursor = "_" if int(t * 2) % 2 == 0 else " "

        if button_up() and time.ticks_ms() - last_up > 250:
            target_scroll_position -= 1
            target_scroll_position = target_scroll_position if target_scroll_position >= 0 else len(applications) - 1
            last_up = time.ticks_ms()
            press_a_label = False
            press_y_label = False
            last_button = t

        if button_down() and time.ticks_ms() - last_down > 250:
            target_scroll_position += 1
            target_scroll_position = target_scroll_position if target_scroll_position < len(applications) else 0
            last_down = time.ticks_ms()
            if press_x_label:
                press_x_label = False
                press_a_label = True
            press_y_label = False
            last_button = t

        if t - last_button >= 2 and not press_x_label and not press_a_label:
            press_y_label = True

        if t - last_button >= 60 and toaster_frames is not None:
            omg_toasters = True

        if button_select():
            # Wait for the button to be released.
            while button_select():
                time.sleep(0.01)

            return applications[selected_item]["file"]

        scroll_position += (target_scroll_position - scroll_position) / 5

        for y in range(int(HEIGHT / 2)):
            bg = background_pen
            sl = scanline_pen
            if y <= 10:
                bg = selected_pen
                sl = selected_scanline_pen
            if y >= int(HEIGHT / 2) - 10:
                bg = unselected_pen
                sl = unselected_scanline_pen
            display.set_pen(bg)
            display.line(0, y * 2, WIDTH, y * 2)
            display.set_pen(sl)
            display.line(0, y * 2 + 1, WIDTH, y * 2 + 1)

        # work out which item is selected (closest to the current scroll position)
        selected_item = round(target_scroll_position)

        text_x = 10
        text_size = 2

        display.set_clip(0, 23, WIDTH, HEIGHT - 44)

        for list_index, application in enumerate(applications):
            distance = list_index - scroll_position

            row_height = text_size * 5 + 20

            # center list items vertically
            text_y = int((HEIGHT / 2) + distance * row_height - (row_height / 2))

            # draw the text, selected item brightest and with shadow
            if selected_item == list_index:
                display.set_pen(shadow_pen)
                display.set_depth(1)
                display.text(application["title"] + cursor, text_x + 1, text_y + 1, -1, text_size)

            selected = selected_item == list_index
            text_pen = selected_pen if selected else unselected_pen
            display.set_pen(text_pen)
            display.text(application["title"] + (cursor if selected else ""), text_x, text_y, -1, text_size)
            display.set_depth(0)

        display.set_clip(0, 0, WIDTH, HEIGHT)

        display.set_pen(background_pen)
        display.text("PicoVisiOS 2.1", text_x, 4, -1, text_size)

        # Hint boxes
        if press_x_label:
            hint(display, text_x, HEIGHT - 14, t, "Press X", selected_pen, shadow_pen)

        if press_a_label:
            hint(display, text_x, HEIGHT - 14, t, "Press A", selected_pen, shadow_pen)

        if press_y_label:
            hint(display, text_x, HEIGHT - 14, t, "Press Y to select", selected_pen, shadow_pen)

        display.update()


# The application we will be launching. This should be ouronly global, so we can
# drop everything else.
application_file_to_launch = menu()

# Run whatever we've set up to.
# If this fails, we'll exit the script and drop to the REPL, which is
# fairly reasonable.
prepare_for_launch()
__import__(application_file_to_launch)
