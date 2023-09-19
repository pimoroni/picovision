# PicoVision boot menu/loader.

import gc
import time
from os import listdir
from picovision import PicoVision, PEN_RGB555
from pimoroni import Button


def hsv_to_rgb(h: float, s: float, v: float) -> tuple[float, float, float]:
    if s == 0.0:
        return v, v, v
    i = int(h * 6.0)
    f = (h * 6.0) - i
    p = v * (1.0 - s)
    q = v * (1.0 - s * f)
    t = v * (1.0 - s * (1.0 - f))
    v = int(v * 255)
    t = int(t * 255)
    p = int(p * 255)
    q = int(q * 255)
    i = i % 6
    if i == 0:
        return v, t, p
    if i == 1:
        return q, v, p
    if i == 2:
        return p, v, t
    if i == 3:
        return p, q, v
    if i == 4:
        return t, p, v
    if i == 5:
        return v, p, q


def get_applications() -> list[dict[str, str]]:
    # fetch a list of the applications that are stored in the filesystem
    applications = []
    for file in listdir():
        if file.endswith(".py") and file != "main.py":
            # convert the filename from "something_or_other.py" to "Something Or Other"
            # via weird incantations and a sprinkling of voodoo
            title = " ".join([v[:1].upper() + v[1:] for v in file[:-3].split("_")])

            applications.append(
                {
                    "file": file,
                    "title": title
                }
            )

    # sort the application list alphabetically by title and return the list
    return sorted(applications, key=lambda x: x["title"])


def prepare_for_launch() -> None:
    for k in locals().keys():
        if k not in ("__name__",
                     "application_file_to_launch",
                     "gc"):
            del locals()[k]
    gc.collect()


def menu() -> str:
    applications = get_applications()

    display = PicoVision(PEN_RGB555, 320, 240)

    button_up = display.is_button_a_pressed
    button_down = display.is_button_x_pressed
    button_select = Button(9, invert=True).read

    WIDTH, HEIGHT = display.get_bounds()

    selected_item = 2
    scroll_position = 2
    target_scroll_position = 2
    last_up = 0
    last_down = 0

    selected_pen = display.create_pen(255, 255, 255)
    unselected_pen = display.create_pen(80, 80, 100)
    shadow_pen = display.create_pen(0, 0, 0)

    while True:
        t = time.ticks_ms() / 1000.0

        if button_up() and time.ticks_ms() - last_up > 250:
            target_scroll_position -= 1
            target_scroll_position = target_scroll_position if target_scroll_position >= 0 else len(applications) - 1
            last_up = time.ticks_ms()

        if button_down() and time.ticks_ms() - last_down > 250:
            target_scroll_position += 1
            target_scroll_position = target_scroll_position if target_scroll_position < len(applications) else 0
            last_down = time.ticks_ms()

        if button_select():
            print("a")
            # Wait for the button to be released.
            while button_select():
                time.sleep(0.01)

            return applications[selected_item]["file"]

        scroll_position += (target_scroll_position - scroll_position) / 5

        grid_size = 40
        for y in range(0, HEIGHT // grid_size):
            for x in range(0, WIDTH // grid_size):
                h = x + y + int(t * 5)
                h = h / 50.0
                r, g, b = hsv_to_rgb(h, 0.5, 1)

                display.set_pen(display.create_pen(r, g, b))
                display.rectangle(x * grid_size, y * grid_size, grid_size, grid_size)

        # work out which item is selected (closest to the current scroll position)
        selected_item = round(target_scroll_position)

        for list_index, application in enumerate(applications):
            distance = list_index - scroll_position

            text_size = 3 if selected_item == list_index else 2

            # center text horixontally
            title_width = display.measure_text(application["title"], text_size)
            text_x = int((WIDTH / 2) - title_width / 2)

            row_height = text_size * 5 + 20

            # center list items vertically
            text_y = int((HEIGHT / 2) + distance * row_height - (row_height / 2))

            # draw the text, selected item brightest and with shadow
            if selected_item == list_index:
                display.set_pen(shadow_pen)
                display.text(application["title"], text_x + 1, text_y - 1, -1, text_size)

            text_pen = selected_pen if selected_item == list_index else unselected_pen
            display.set_pen(text_pen)
            display.text(application["title"], text_x, text_y, -1, text_size)

        display.update()


# The application we will be launching. This should be ouronly global, so we can
# drop everything else.
application_file_to_launch = menu()

# Run whatever we've set up to.
# If this fails, we'll exit the script and drop to the REPL, which is
# fairly reasonable.
prepare_for_launch()
__import__(application_file_to_launch)
