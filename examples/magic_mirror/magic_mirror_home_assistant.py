"""
MicroMagicMirror - a home dashboard that shows the time, weather and data from the Home Assistant REST API.

To get online data, create a file called secrets.py containing your WiFi SSID and password, like this:
WIFI_SSID = "ssid_goes_here"
WIFI_PASSWORD = "password_goes_here"

To connect to Home Assistant, you'll need to create a token from http://homeassistant.local:8123/profile > Long Lived Access Tokens
and add it to your secrets.py:
HOME_ASSISTANT_TOKEN = "token_goes_here"

Solar data is provided by the Home Assistant Sun integration. This should be installed by default.

Press the Y button to update the data.
"""

from modes import VGA
from pimoroni import Button
import pngdec
import urequests
import ntptime
import time
import machine
import gc
import math
import random

# General settings:
# set this to True if you don't want your mirror to display your WiFi password
REDACT_WIFI = True

# Adjust the clock to show the correct timezone
UTC_OFFSET = 1.0

# how often to poll the APIs for data, in minutes
UPDATE_INTERVAL = 15

# Home Assistant settings:
HOME_ASSISTANT_URL_1 = "http://homeassistant.local:8123/api/states/sensor.sun_next_setting"
HOME_ASSISTANT_URL_2 = "http://homeassistant.local:8123/api/states/sensor.sun_next_rising"

# Constants for drawing the display
TITLE = "MicroMagicMirror"
TITLE_POSITION = 20
ROW_1_POSITION = 72
ROW_2_POSITION = 110
ROW_3_POSITION = 200
ROW_4_POSITION = 330
CLOCK_POSITION = 430
COLUMN_2_POSITION = 80
COLUMN_3_POSITION = 530
PADDING = 10
ICON_OFFSET = 20

# set up our PicoGraphics display and some useful constants
graphics = VGA()

WIDTH, HEIGHT = graphics.get_bounds()

BLACK = graphics.create_pen(0, 0, 0)
RED = graphics.create_pen(255, 0, 0)
ORANGE = graphics.create_pen(246, 138, 30)
YELLOW = graphics.create_pen(255, 216, 0)
GREEN = graphics.create_pen(0, 121, 64)
BLUE = graphics.create_pen(0, 0, 255)
INDIGO = graphics.create_pen(36, 64, 142)
VIOLET = graphics.create_pen(115, 41, 130)
WHITE = graphics.create_pen(255, 255, 255)
GREY = graphics.create_pen(128, 128, 128)

# Create a new PNG decoder for our PicoGraphics
png = pngdec.PNG(graphics)

# create the rtc object
rtc = machine.RTC()

sensor_temp = machine.ADC(4)
conversion_factor = 3.3 / (65535)  # used for calculating a temperature from the raw sensor reading

button_y = Button(9)

# some other variables for keeping track of stuff
timer = 0
second = 0
last_second = 0
temperature = None
wifi_problem = False

BUILDING_COUNT = 20
buildings = [random.randint(0, HEIGHT // 6) for _ in range(WIDTH // BUILDING_COUNT)]

STAR_COUNT = 50
stars = [(random.randint(0, WIDTH), random.randint(0, HEIGHT)) for _ in range(STAR_COUNT)]


# Connect to wifi
def connect_to_wifi():
    global WIFI_SSID, WIFI_PASSWORD
    import network
    try:
        from secrets import WIFI_SSID, WIFI_PASSWORD
    except ImportError:
        print("Create secrets.py with your WiFi credentials")
        return False

    wlan = network.WLAN(network.STA_IF)
    if wlan.isconnected():
        return True

    wlan.active(True)
    wlan.config(pm=0xa11140)  # Turn WiFi power saving off for some slow APs
    retries = 0

    MAX_RETRIES = 5  # How many times to retry connecting to WiFi before giving up

    while retries < MAX_RETRIES:
        print(f"Attempt {retries + 1} to connect to WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)

        max_wait = 100
        while max_wait > 0:
            if wlan.status() < 0 or wlan.status() >= 3:
                break
            max_wait -= 1
            time.sleep(0.2)

        if wlan.isconnected():
            print("Connected to WiFi successfully.")
            return True
        print("Failed to connect to WiFi. Retrying...")
        retries += 1
        time.sleep(2)  # Wait for 2 seconds before retrying

    print("Failed to connect to WiFi after maximum retries.")
    return False


# Synchronize the RTC time from NTP and download data from our APIs
def get_data():
    global wifi_problem
    if connect_to_wifi() is False:
        wifi_problem = True
        return
    wifi_problem = False

    try:
        print("Setting time from NTP")
        ntptime.settime()
        print(f"Time set (UTC): {rtc.datetime()}")
    except OSError as e:
        print(e)

    get_home_assistant_data()


def get_home_assistant_data():
    global sunset_date, sunset_time, sunrise_date, sunrise_time
    # Home Assistant's API requires us to supply the authorisation token via headers
    from secrets import HOME_ASSISTANT_TOKEN
    headers = {
        "Authorization": "Bearer " + HOME_ASSISTANT_TOKEN,
        "content-type": "application/json",
    }

    print("Requesting Home Assistant data")
    try:
        r = urequests.get(HOME_ASSISTANT_URL_1, headers=headers, timeout=5)
        j = r.json()
        print("Home Assistant data obtained:")
        print(j)
        sunset_date, sunset_time = j["state"].split("T")
        r.close()
    except OSError as e:
        print(e)

    print("Requesting Home Assistant data")
    try:
        r = urequests.get(HOME_ASSISTANT_URL_2, headers=headers, timeout=5)
        j = r.json()
        print("Home Assistant data obtained:")
        print(j)
        sunrise_date, sunrise_time = j["state"].split("T")
        r.close()
    except OSError as e:
        print(e)


def redraw_display_if_reqd():
    global year, month, day, wd, hour, minute, second, last_second
    if second != last_second:
        # clear the screen
        graphics.set_pen(BLACK)
        graphics.clear()

        # draw a lovely rainbow
        graphics.set_pen(VIOLET)
        graphics.circle(0, 0, 140)
        graphics.set_pen(BLUE)
        graphics.circle(0, 0, 120)
        graphics.set_pen(GREEN)
        graphics.circle(0, 0, 100)
        graphics.set_pen(YELLOW)
        graphics.circle(0, 0, 80)
        graphics.set_pen(ORANGE)
        graphics.circle(0, 0, 60)
        graphics.set_pen(RED)
        graphics.circle(0, 0, 40)

        # draw the title
        graphics.set_pen(WHITE)
        graphics.text(TITLE, COLUMN_2_POSITION, TITLE_POSITION, scale=7)

        # show system temperature
        # the following two lines do some maths to convert the number from the temp sensor into celsius
        reading = sensor_temp.read_u16() * conversion_factor
        internal_temperature = 27 - (reading - 0.706) / 0.001721
        graphics.text(f"uMM is running at {internal_temperature:.1f}°C.", COLUMN_2_POSITION, ROW_1_POSITION, WIDTH - COLUMN_2_POSITION, scale=2)

        # show wifi details
        graphics.set_pen(WHITE)
        if wifi_problem is True:
            png.open_file("/magic_mirror/icons/no-wifi.png")
            graphics.text("WiFi not available. Create secrets.py to connect to WiFi!", PADDING, ROW_2_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
        else:
            png.open_file("/magic_mirror/icons/wifi.png")
            if REDACT_WIFI is True:
                graphics.text(f"Our WiFi network is '{WIFI_SSID}'.", PADDING, ROW_2_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
            else:
                graphics.text(f"Our WiFi network is '{WIFI_SSID}' and the password is '{WIFI_PASSWORD}'.", PADDING, ROW_2_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
        png.decode(COLUMN_3_POSITION, ROW_2_POSITION - ICON_OFFSET)

        if sunrise_date and sunset_date is not None:
            # Convert the sunrise, sunset and RTC times to a common 'minutes since midnight' format, so we can compare:
            sunrise_split = sunrise_time.split(":")
            sunset_split = sunset_time.split(":")
            sunrise_minutes = (int(sunrise_split[0]) + UTC_OFFSET) * 60 + int(sunrise_split[1])
            sunset_minutes = (int(sunset_split[0]) + UTC_OFFSET) * 60 + int(sunset_split[1])
            rtc_minutes = (rtc.datetime()[4] + UTC_OFFSET) * 60 + rtc.datetime()[5]

            if sunrise_minutes <= rtc_minutes <= sunset_minutes:
                # The sun is up!
                # Now work out how far through the day we are
                sun_progression = (rtc_minutes - sunrise_minutes) / (sunset_minutes - sunrise_minutes)
                # Use the progression to work out the x and y
                x = int(sun_progression * WIDTH)
                # Some maths to give the sun a nice curve, moving between HEIGHT / 4 and 2 * HEIGHT / 4
                y = (HEIGHT * 1/4) * (1 + math.sin(sun_progression * 3.14))
                # draw the sun
                graphics.set_pen(YELLOW)
                graphics.circle(int(x), int(HEIGHT - y), 30)
                # add the text
                graphics.set_pen(WHITE)
                graphics.text(f"Sun will set at {int(sunset_minutes / 60):02}:{int(sunset_minutes % 60):02} on {sunset_date}.", PADDING, ROW_3_POSITION, WIDTH, scale=3)
            else:
                # The sun is down!
                # draw some stars
                graphics.set_pen(GREY)
                for s in stars:
                    graphics.circle(s[0], s[1], 2)
                # add the text
                graphics.set_pen(WHITE)
                graphics.text(f"Sun will rise at {int(sunrise_minutes / 60):02}:{int(sunrise_minutes % 60):02} on {sunrise_date}.", PADDING, ROW_3_POSITION, WIDTH, scale=3)
            # draw some buildings
            graphics.set_pen(GREY)
            for i in range(BUILDING_COUNT):
                graphics.rectangle(i * WIDTH // BUILDING_COUNT, HEIGHT - 50 - buildings[i], WIDTH // BUILDING_COUNT, HEIGHT - 50)
            # draw a green rectangle to represent the ground
            graphics.set_pen(GREEN)
            graphics.rectangle(0, HEIGHT - 50, WIDTH, 50)

        # draw clock
        graphics.set_pen(WHITE)
        graphics.text(f"{hour:02}:{minute:02}:{second:02}", PADDING, CLOCK_POSITION, scale=7)
        text_width = graphics.measure_text(f"{day}/{month}/{year}", scale=7)
        graphics.text(f"{day}/{month}/{year}", WIDTH - text_width, CLOCK_POSITION, scale=7)

        last_second = second
        graphics.update()


# Display 'updating' message when first started up
graphics.set_font("bitmap8")
graphics.set_pen(BLACK)
graphics.clear()
graphics.set_pen(WHITE)
graphics.text("Updating!", PADDING, CLOCK_POSITION, scale=6)
graphics.update()

get_data()
redraw_display_if_reqd()
last_updated = time.ticks_ms()

while True:
    # get new data when button Y is pressed
    if button_y.is_pressed:
        get_data()
        last_updated = time.ticks_ms()

    ticks_now = time.ticks_ms()
    # get new data after UPDATE_INTERVAL has passed
    if time.ticks_diff(ticks_now, last_updated) > UPDATE_INTERVAL * 1000 * 60:
        get_data()
        last_updated = time.ticks_ms()

    year, month, day, wd, hour, minute, second, _ = rtc.datetime()
    # apply timezone offset
    hour = (hour + UTC_OFFSET) % 24

    redraw_display_if_reqd()

    # clean up
    gc.collect()
