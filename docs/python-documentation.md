# PicoVision: MicroPython Documentation <!-- omit in toc -->

- [Getting Started](#getting-started)
  - [Differences from PicoGraphics](#differences-from-picographics)
- [Display Resolutions \& Pen Types](#display-resolutions--pen-types)
  - [Modes](#modes)
  - [Pen Colour \& Background Colour](#pen-colour--background-colour)
  - [Pen Depth](#pen-depth)
- [Sprites](#sprites)
  - [Loading Sprites](#loading-sprites)
  - [Loading Animations](#loading-animations)
  - [Displaying Sprites](#displaying-sprites)
- [Advanced Features](#advanced-features)
  - [Scanlines](#scanlines)
  - [Frame vs Display](#frame-vs-display)
- [GPIO](#gpio)

## Getting Started

### Differences from PicoGraphics

PicoVision is loosely based on [PicoGraphics](https://github.com/pimoroni/pimoroni-pico/blob/main/micropython/modules/picographics/README.md), but has some caveats that apply uniquely to PicoVision.

Most notably, PicoVision is backed by two *physical* PSRAM buffers, which also serve double-duty as storage for sprite data.

This means that when you load sprites you must load them into both buffers, the [Loading Sprites](#loading-sprites) goes into this in more detail.

It also means that the buffer you get back from the GPU will usually be "stale." This means it's filled with the contents of the frame before the last frame you sent to the GPU.

## Display Resolutions & Pen Types

See [Hardware - Display Resolutions & Pen Types](hardware.md#display-resolutions--pen-types) for a list of available resolutions and modes.

When initialising PicoGraphics for PicoVision you'll need to specify three things:

* pen type
* width
* height

Pen types are available as constants: `PEN_RGB888`, `PEN_RGB555` and `PEN_P5`.

* `PEN_RGB888` - Eight bits per channel, 16m colours
* `PEN_RGB555` - Five bits per channel, 32k colours
* `PEN_P5` - Five bits per pixel, paletted, 32 colours

The width and height you must specify yourself based on your project, eg:

```python
from picovision import PicoVision, PEN_RGB555

display = PicoVision(PEN_RGB555, 640, 480)
```

Higher resolutions at larger pen types will - usually - cost more to redraw.

### Modes

PicoVision includes a `modes` library with some common resolutions for your convenience, eg:

```python
from modes import VGA

display = VGA()
```

### Pen Colour & Background Colour

Like PicoGraphics, PicoVision requires you to `set_pen(colour)` to choose the drawing colour. To create a colour you must first use `create_pen(r, g, b)`. In RGB888 and RGB555 modes this will create the appropriate, packed colour and in P5 mode this will - if there's space available - add an RGB888 colour to the palette and return its index-

```python
BLUE = display.create_pen(0, 0, 255)
display.set_pen(BLUE)
```

PicoVision includes a `set_bg(colour)` method for setting a background colour for blend operations, such as PicoVector's Anti-Aliasing. This can speed up blending where you have a known background colour and want to save the cost of reading it back from PSRAM. In order to use the fixed colour instead of the target pixel colour, you should set the blend mode to `BLEND_FIXED`:

```python
display.set_blend_mode(BLEND_FIXED)
```

For normal blending you should use `BLEND_TARGET`:

```python
display.set_blend_mode(BLEND_TARGET)
```

### Pen Depth

In addition to colour, you can set the "depth" of a pen, putting drawn pixels behind sprites depending upon their blend mode.

## Sprites

First, some terms-

* Image Index - an index to identify where sprite image data should be stored
* Sprite Slot - a single, hardware sprite. There are up to 80 (or 32 on widescreen versions),
  but note the hard limit of 10 simultaneous sprites per scanline as described in [Hardware - Sprites](hardware.md#sprites).

### Loading Sprites

An index must be specified when loading an image, and can later be used to refer to that specific image and assign it to a sprite. It's a good idea to use a variable to keep track of these:

```python
IMAGE_ELEPHANT = 1
for _ in range(2):
    display.load_sprite("elephant.png", IMAGE_ELEPHANT)
    display.update()
```

Note: the quirk of loading the sprite twice in a loop is because we must swap the hardware PSRAM buffers and load the image data into each.

If you omit the image index from `load_sprite` it will return a width, height and the raw, unpacked data bytes - in your current pen type - of the loaded .png file as a tuple. This is useful for loading sprite data into memory to avoid the cost of PNG decoding, cache to flash or otherwise.

### Loading Animations

An animation usually requires a number of frames, so `load_animation()` is supplied to make loading them into PSRAM easier.

For example, the following code would load four 32x32 frames from `my_character.png` and return a list of image indexes starting at the one supplied:

```python
FRAMES = display.load_animation(IMAGE_FRAME_1, "my_character.png", (32, 32), source=(0, 0, 32, 128))
```

You can use the `source` argument to specify a region of your image file to load, and the third argument - in this case given as `(32, 32)` - should be a tuple describing the width and height of your frames.

### Displaying Sprites

The `display_sprite` function serves both to display a sprite and to update its location or image:

```python
SPRITE_SLOT = 1
display.display_sprite(SPRITE_SLOT, IMAGE_ELEPHANT, X, Y, blend_mode=SPRITE_OVERWRITE, v_scale=1)
```

The sprite slot refers to a single "hardware" sprite. Each of these slots can display a specific image - referenced by its index - at a given X and Y position and with optional vertical scaling.

The `blend` mode refers to one of the [sprite blend modes](hardware.md#sprite-blending), constants are provided for you:

* `SPRITE_OVERWRITE` - 0
* `SPRITE_UNDER` - 1
* `SPRITE_OVER` - 2
* `SPRITE_BLEND_UNDER` - 3
* `SPRITE_BLEND_OVER` - 4

Once displayed the sprite will continue to display at its given X and Y coordinates without any intervention from your code. To remove it you need to clear it:

```python
display.clear_sprite(SPRITE_SLOT)
```

Sprite images can be up to 64x32 big, but if the data is larger than 2kB then the following image index is not usable.  For example, if you load a 4kB (64x32) sprite into image index 1 you must not use image index 2.

## Advanced Features

### Scanlines

You can configure the "GPU" to scroll up to seven groups of scanlines in groups 1 to 7. The following code cuts a 240 pixel tall frame into three horizontal slices:

```python
display.set_scroll_group_for_lines(1, 0, 80)
display.set_scroll_group_for_lines(2, 80, 160)
display.set_scroll_group_for_lines(3, 160, 240)
```

By default all lines are in scroll group 0 (which can't scroll).

Once the scroll groups have been configured, you can ask the "GPU" to offset them like so:

```python
display.set_scroll_group_offset(1, X, Y)
```

The first argument is the scroll group (in the range 1 to 7).

You can also wrap the scroll, by specifying the maximum X and Y values within the frame, and optionally the X and Y value to wrap back to (0,0 by default).

```python
display.set_scroll_group_offset(1, X, Y, WRAP_X_AT, WRAP_Y_AT, WRAP_X_TO, WRAP_Y_TO)
```

This can be used to continuously scroll an image that is the width of the display or larger by setting WRAP_X to the frame width and slowly increasing the X value.  For example:

```python
scroll = int(time.ticks_ms() / 60) % DISPLAY_WIDTH
display.set_scroll_group_offset(1, scroll, 0, DISPLAY_WIDTH)
```

### Frame vs Display

The `PicoGraphics` constructor can optionally take `frame_width` and `frame_height` arguments which specify a larger drawing area that you can pan around with scroll offsets. This is useful to mask slow drawing of new elements, or to contain additional horizontal data that the scanline scrolling could bring into view.

For example the following code would output a 320x240 image, but you would be drawing into a 960x240 canvas:

```python
DISPLAY_WIDTH = 320 
DISPLAY_HEIGHT = 240

FRAME_WIDTH = DISPLAY_WIDTH * 3
FRAME_HEIGHT = 240

display = PicoVision(pen_type=PEN,
                     width=DISPLAY_WIDTH,
                     height=DISPLAY_HEIGHT,
                     frame_width=FRAME_WIDTH,
                     frame_height=FRAME_HEIGHT)
```

With a larger canvas you can draw offscreen and use scroll offsets to bring those regions into view.

## GPIO

The Y button is connected to GPIO 9 on the Pico W and can be used in the normal way, it should be pulled up and is low when pressed:
```python
button_y = machine.Pin(9, machine.Pin.IN, machine.Pin.PULL_UP)
if button_y.value() == 0:
    print("Y is pressed")
```

The A and X buttons are connected to the GPU and can be read with functions on the PicoVision display class:

```python
if display.is_button_x_pressed():
    print("X is pressed")
if display.is_button_a_pressed():
    print("A is pressed")
```

The GPU GPIOs on the header are also accessed through the display class:
```python
display.get_gpu_io_value(0)                # Get the value of GPU GPIO 0 (CK)
display.set_gpu_io_output_enable(1, True)  # Enable output on GPU GPIO 1 (CS)
display.set_gpu_io_value(1, True)          # Set GPU GPIO 1 (CS) high
display.set_gpu_io_pull_up(2, True)        # Enable pull up on GPU GPIO 2 (D0)
display.set_gpu_io_pull_down(3, True)      # Enable pull down on GPU GPIO 3 (D1)
```

GPIO 29 can be used as a PWM, if you enable it as an output and set a float value between 0 and 1 it will be set to that duty cycle:
```python
display.set_gpu_io_output_enable(29, True)  # Enable output on GPU GPIO 29
display.set_gpu_io_value(29, 0.25)          # Set GPU GPIO 29 on 25% of the time
```

GPIO 29 can also be used as an ADC, to read a voltage between 0 and 3.3V.  There will be a short delay (around 1.5ms) between the ADC being enabled and the first reading being available.
```python
display.set_gpu_io_adc_enable(29, True)
time.sleep(0.002)
voltage = display.get_gpu_io_adc_voltage(29)
print(f"The voltage on pin 29 is {voltage:.02f}V")
```