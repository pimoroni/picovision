"""
MicroMagicMirror - a home dashboard that shows the time, weather and an inspirational quote.

To get online data, create a file called secrets.py containing your WiFi SSID and password, like this:
WIFI_SSID = "ssid_goes_here"
WIFI_PASSWORD = "password_goes_here"

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

# General settings:
# set this to True if you don't want your mirror to display your WiFi password
REDACT_WIFI = True

# Adjust the clock to show the correct timezone
UTC_OFFSET = 1.0

# how often to poll the APIs for data, in minutes
UPDATE_INTERVAL = 15

# Weather settings:
# Weather from OpenMeteo - find out more at https://open-meteo.com/

# Set your latitude/longitude here (find yours by right clicking in Google Maps!)
LAT = 53.38609085276884
LNG = -1.4239983439328177
TIMEZONE = "auto"  # determines time zone from lat/long
WEATHER_URL = "http://api.open-meteo.com/v1/forecast?latitude=" + str(LAT) + "&longitude=" + str(LNG) + "&current_weather=true&timezone=" + TIMEZONE

# Quote settings:
# Quotes are from quotable.io - API documentation here: https://github.com/lukePeavey/quotable
QUOTE_URL = "https://api.quotable.io/quotes/random?tags=technology|education|future&maxLength=155"

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

# set up our display and some useful constants
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
quote = None
wifi_problem = False


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

    get_weather_data()

    get_quote_data()


def get_weather_data():
    global temperature, windspeed, winddirection, weathercode, date, now
    try:
        print("Requesting weather data")
        r = urequests.get(WEATHER_URL, timeout=5)
        # open the json data
        j = r.json()
        print("Weather data obtained:")
        print(j)
        # parse relevant data from JSON
        current = j["current_weather"]
        temperature = current["temperature"]
        windspeed = current["windspeed"]
        winddirection = calculate_bearing(current["winddirection"])
        weathercode = current["weathercode"]
        date, now = current["time"].split("T")
        r.close()
    except OSError as e:
        print(e)


def get_quote_data():
    global quote, author
    try:
        print("Requesting quote data")
        r = urequests.get(QUOTE_URL, timeout=5)
        # open the json data
        j = r.json()
        print("Quote data obtained:")
        print(j)
        # parse relevant data from JSON
        quote = j[0]["content"]
        author = j[0]["author"]
        r.close()
    except OSError as e:
        print(e)


def calculate_bearing(d):
    # calculates a compass direction from the wind direction in degrees
    dirs = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]
    ix = round(d / (360. / len(dirs)))
    return dirs[ix % len(dirs)]


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

        # lets have some weather
        graphics.set_pen(WHITE)
        if temperature is not None:
            # Choose an appropriate icon based on the weather code
            # Weather codes from https://open-meteo.com/en/docs
            # Weather icons from https://icons8.com/
            if weathercode in [71, 73, 75, 77, 85, 86]:  # codes for snow
                png.open_file("/magic_mirror/icons/snow.png")
            elif weathercode in [51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82]:  # codes for rain
                png.open_file("/magic_mirror/icons/rain.png")
            elif weathercode in [1, 2, 3, 45, 48]:  # codes for cloud
                png.open_file("/magic_mirror/icons/cloud.png")
            elif weathercode in [0]:  # codes for sun
                png.open_file("/magic_mirror/icons/sun.png")
            elif weathercode in [95, 96, 99]:  # codes for storm
                png.open_file("/magic_mirror/icons/storm.png")
            png.decode(COLUMN_3_POSITION, ROW_2_POSITION - ICON_OFFSET)
            # draw the weather text
            graphics.set_pen(WHITE)
            graphics.text(f"Outside, the temperature is {temperature}°C. The wind speed is {windspeed}kmph {winddirection}.", PADDING, ROW_2_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
            graphics.text(f"Last OpenMeteo data: {now}, {date}", PADDING, ROW_2_POSITION + 55, scale=2)
        else:
            graphics.text("Weather data not available.", PADDING, ROW_2_POSITION, scale=3)

        # and a quote
        graphics.set_pen(YELLOW)
        if quote is not None:
            graphics.text(f"'{quote}' - {author}", PADDING, ROW_3_POSITION, WIDTH - PADDING, scale=3)
        else:
            graphics.text("Quote data not available.", PADDING, ROW_3_POSITION, WIDTH - PADDING, scale=3)

        # show wifi details
        graphics.set_pen(WHITE)
        if wifi_problem is True:
            png.open_file("/magic_mirror/icons/no-wifi.png")
            graphics.text("WiFi not available. Create secrets.py to connect to WiFi!", PADDING, ROW_4_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
        else:
            png.open_file("/magic_mirror/icons/wifi.png")
            if REDACT_WIFI is True:
                graphics.text(f"Our WiFi network is '{WIFI_SSID}'.", PADDING, ROW_4_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
            else:
                graphics.text(f"Our WiFi network is '{WIFI_SSID}' and the password is '{WIFI_PASSWORD}'.", PADDING, ROW_4_POSITION, COLUMN_3_POSITION - PADDING, scale=3)
        png.decode(COLUMN_3_POSITION, ROW_4_POSITION - ICON_OFFSET)

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
        connect_to_wifi()
        last_updated = time.ticks_ms()

    year, month, day, wd, hour, minute, second, _ = rtc.datetime()
    # apply timezone offset
    hour = (hour + UTC_OFFSET) % 24

    redraw_display_if_reqd()

    # clean up
    gc.collect()
