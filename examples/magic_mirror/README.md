- [Micro Magic Mirror](#micro-magic-mirror)
  - [About](#about)
  - [Icons](#icons)
  - [Variants](#variants)
    - [Magic Mirror](#magic-mirror)
    - [Magic Mirror Without Wifi](#magic-mirror-without-wifi)
    - [Magic Mirror (Home Assistant)](#magic-mirror-home-assistant)

# Micro Magic Mirror

## About

Micro Magic Mirror is a home dashboard for showing you useful information. We're working on an associated learn guide to show you how we built our Magic Mirror.

To get online data, create a file called secrets.py containing your WiFi SSID and password, like this:
```
WIFI_SSID = "ssid_goes_here"
WIFI_PASSWORD = "password_goes_here"
```
Press the Y button to update the online data.

## Icons

Weather/wifi icons are from [Icons8](https://icons8.com).

## Variants

There are three variants of this example in this directory, showing various different functions. For an example that will do something out of the box without any configuring, go for `magic_mirror.py`.

### Magic Mirror

[magic_mirror.py](magic_mirror.py)

This example connects to wifi and shows you the time (from an NTP server), the current weather conditions (from the OpenMeteo API) and an inspirational quote (from quotable.io).

### Magic Mirror Without Wifi

[magic_mirror_without_wifi.py](magic_mirror_without_wifi.py)

This stripped down version shows how we're drawing the basic framework of our Magic Mirror display, before we start adding in the wifi functions. It has a graph showing the temperature from the internal sensor on the Pico W.

### Magic Mirror (Home Assistant)

[magic_mirror_home_assistant.py](magic_mirror_home_assistant.py)

Do you automate your home with [Home Assistant](https://www.home-assistant.io/)? (if not you should try it, it's great fun). This version of Magic Mirror displays sunrise/sunset data it gets from a local Home Assistant server.

To connect to your Home Assistant, you'll need to create a token at http://homeassistant.local:8123/profile > Long Lived Access Tokens and add it to your `secrets.py` like this:
```
HOME_ASSISTANT_TOKEN = "token_goes_here"
```
Solar data is provided by the Home Assistant Sun integration, which should be installed by default. The example assumes your Pico W is connected to the same network as your Home Assistant server, and that your Home Assistant server is located at http://homeassistant.local:8123