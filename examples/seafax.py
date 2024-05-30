# News headlines, Pirate style!

# You will need to create or update the file secrets.py with your network credentials using Thonny
# in order for the example to connect to WiFi.

# secrets.py should contain:
# WIFI_SSID = ""
# WIFI_PASSWORD = ""

from picographics import PicoGraphics, PEN_RGB555
from pimoroni import Button
import time
import network
import ntptime
import machine
from urllib import urequest
import gc
try:
    from secrets import WIFI_SSID, WIFI_PASSWORD
except ImportError:
    print("Create secrets.py with your WiFi credentials")

# Uncomment one URL to use (Top Stories, World News and Technology)
# URL = "https://feeds.bbci.co.uk/news/rss.xml"
# URL = "https://feeds.bbci.co.uk/news/world/rss.xml"
URL = "https://feeds.bbci.co.uk/news/technology/rss.xml"

display = PicoGraphics(pen_type=PEN_RGB555, width=640, height=480)

WIDTH, HEIGHT = display.get_bounds()

BG = display.create_pen(0, 0, 0)
BLUE = display.create_pen(0, 0, 255)
YELLOW = display.create_pen(255, 255, 0)
WHITE = display.create_pen(255, 255, 255)
RED = display.create_pen(255, 0, 0)
GREEN = display.create_pen(0, 255, 0)
CYAN = display.create_pen(0, 255, 255)

rtc = machine.RTC()

button_y = Button(9)

# Enable the Wireless
wlan = network.WLAN(network.STA_IF)
wlan.active(True)


def network_connect(ssid, psk):
    # Number of attempts to make before timeout
    max_wait = 5

    print("connecting...")
    wlan.config(pm=0xa11140)  # Turn WiFi power saving off for some slow APs
    wlan.connect(ssid, psk)

    while max_wait > 0:
        if wlan.status() < 0 or wlan.status() >= 3:
            break
        max_wait -= 1
        print('waiting for connection...')
        time.sleep(1)

    # Handle connection error.
    if wlan.status() != 3:
        print("Unable to connect.")


# Function to sync the Pico RTC using NTP
def sync_time():
    try:
        network_connect(WIFI_SSID, WIFI_PASSWORD)
    except NameError:
        print("Create secrets.py with your WiFi credentials")
    try:
        ntptime.settime()
    except OSError:
        print("Unable to sync with NTP server. Check network and try again.")


def read_until(stream, find):
    result = b""
    while len(c := stream.read(1)) > 0:
        if c == find:
            return result
        result += c


def discard_until(stream, find):
    _ = read_until(stream, find)


def parse_xml_stream(s, accept_tags, group_by, max_items=7):
    tag = []
    text = b""
    count = 0
    current = {}

    while True:
        char = s.read(1)
        if len(char) == 0:
            break

        if char == b"<":
            next_char = s.read(1)

            # Discard stuff like <?xml vers...
            if next_char == b"?":
                discard_until(s, b">")
                continue

            # Detect <![CDATA
            elif next_char == b"!":
                s.read(1)  # Discard [
                discard_until(s, b"[")  # Discard CDATA[
                text = read_until(s, b"]")
                discard_until(s, b">")  # Discard ]>
                gc.collect()

            elif next_char == b"/":
                current_tag = read_until(s, b">")
                top_tag = tag[-1]

                # Populate our result dict
                if top_tag in accept_tags:
                    current[top_tag.decode("utf-8")] = text.decode("utf-8")

                # If we've found a group of items, yield the dict
                elif top_tag == group_by:
                    yield current
                    current = {}
                    count += 1
                    if count == max_items:
                        return
                tag.pop()
                text = b""
                gc.collect()
                continue

            else:
                current_tag = read_until(s, b">")
                if not current_tag.endswith(b"/"):
                    tag += [next_char + current_tag.split(b" ")[0]]
                    text = b""

        else:
            text += char


def get_rss():
    try:
        stream = urequest.urlopen(URL)
        output = list(parse_xml_stream(stream, [b"title", b"description", b"guid", b"pubDate"], b"item"))
        return output

    except OSError as e:
        print(e)
        return False


def update():
    global feed
    # Gets Feed Data
    feed = get_rss()


def sea_header():
    display.set_pen(WHITE)
    display.set_font("bitmap8")

    current_t = rtc.datetime()
    header_string = "P101    SEAFAX          {day:02d}/{month:02d}/{year}".format(day=current_t[2], month=current_t[1], year=current_t[0])
    time_string = "{hour:02d}:{minute:02d}/{seconds:02d}".format(hour=current_t[4], minute=current_t[5], seconds=current_t[6])

    display.text(header_string, 10, 5, WIDTH, 1)
    display.set_pen(YELLOW)
    display.text(time_string, WIDTH - 40, 5, WIDTH, 1)

    display.set_pen(WHITE)
    display.text("PIM", 10, 20, WIDTH, 7)
    w = display.measure_text("PIM", 7)

    display.set_pen(BLUE)
    display.rectangle(w + 10, 20, 550, 50)

    display.set_pen(BG)
    display.text("HEADLINES", w + 15, 25, WIDTH, 6)
    display.set_pen(YELLOW)
    display.text("HEADLINES", w + 15, 23, WIDTH, 6)


feed = None

sync_time()
update()

while True:
    # get new data when button Y is pressed
    if button_y.is_pressed:
        sync_time()
        update()

    display.set_pen(BG)
    display.clear()

    sea_header()

    if feed:
        display.set_pen(WHITE)
        display.text(feed[0]["title"], 10, 100, WIDTH - 50, 2)
        display.text(feed[1]["title"], 10, 150, WIDTH - 50, 2)
        display.text(feed[2]["title"], 10, 200, WIDTH - 50, 2)
        display.text(feed[3]["title"], 10, 250, WIDTH - 50, 2)
        display.text(feed[4]["title"], 10, 300, WIDTH - 50, 2)
        display.text(feed[5]["title"], 10, 350, WIDTH - 50, 2)
        display.text(feed[6]["title"], 10, 400, WIDTH - 50, 2)
    else:
        display.set_pen(BLUE)
        display.rectangle(0, (HEIGHT // 2) - 20, WIDTH, 40)
        display.set_pen(YELLOW)
        display.text("Unable to display news feed!", 5, (HEIGHT // 2) - 15, WIDTH, 2)
        display.text("Check your network settings in secrets.py", 5, (HEIGHT // 2) + 2, WIDTH, 2)

    display.set_pen(BLUE)
    display.rectangle(0, HEIGHT - 30, WIDTH, 10)
    display.set_pen(YELLOW)
    display.text("PicoVision by Pimoroni", 5, HEIGHT - 29, WIDTH, 1)
    display.set_pen(RED)
    display.text("PIRATE", 5, HEIGHT - 15, WIDTH, 1)
    display.set_pen(GREEN)
    display.text("MONKEY", 200, HEIGHT - 15, WIDTH, 1)
    display.set_pen(YELLOW)
    display.text("ROBOT", 400, HEIGHT - 15, WIDTH, 1)
    display.set_pen(CYAN)
    display.text("NINJA", WIDTH - 25, HEIGHT - 15, WIDTH, 1)

    display.update()
    time.sleep(0.01)
