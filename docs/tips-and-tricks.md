# PicoVision: Tips & Tricks

## "Hardware" Scrolling

PicoVision allows you to work with a drawing canvas that's larger than your screen and apply up to three scroll offsets to groups of scanlines.

Each scanline can be assigned to a scroll group, and there's no requirement for these to be contigous.

To set up scroll offsets you must use the `set_scroll_group_for_lines` method, this takes a group number (0-3, group 0 can't be scrolled) and the lines you want to assign to it.

For example, slicing a 320x240 screen into three equal portions, each with their own scroll group, might look like this:

```python
display.set_scroll_group_for_lines(1, 0, 80)
display.set_scroll_group_for_lines(2, 80, 160)
display.set_scroll_group_for_lines(3, 160, 240)
```

To scroll a portion of the screen, you must use the `set_scroll_group_offset` function. This takes the scroll group to offset and the  X and Y values to scroll it: 

```python
display.set_scroll_group_offset(1, x, y)
display.set_scroll_group_offset(2, x, y)
display.set_scroll_group_offset(3, x, y)
```

If we put this together, we can scroll three lines of text across the screen without having to pay the cost of drawing them every frame:

```python
import time
import math

from picographics import PicoGraphics, PEN_RGB555

WIDTH = 320
HEIGHT = 240

display = PicoGraphics(pen_type=PEN_RGB555, width=WIDTH, height=HEIGHT, frame_width=WIDTH * 2, frame_height=HEIGHT)

TEXT = "Hello World"
TEXT_SCALE = 3

BLACK = display.create_pen(0, 0, 0)
WHITE = display.create_pen(255, 255, 255)

# Split the display into three scrollable regions of equal size
display.set_scroll_group_for_lines(1, 0, 80)
display.set_scroll_group_for_lines(2, 80, 160)
display.set_scroll_group_for_lines(3, 160, 240)

# Calculate the position of the text
text_width = display.measure_text(TEXT, scale=TEXT_SCALE)
text_height = 5 * TEXT_SCALE

top = int(80 / 2) - text_height

left = int((WIDTH - text_width) / 2)
left += int(WIDTH / 2)

# Calculate how much we should move the text to swoosh it back and forth
offset = (WIDTH - text_width) / 2

# Draw the display, just once, and flip the buffer to the "GPU"
display.set_pen(BLACK)
display.clear()
display.set_pen(WHITE)
display.text(TEXT, left, 0 + top, scale=TEXT_SCALE)
display.text(TEXT, left, 80 + top, scale=TEXT_SCALE)
display.text(TEXT, left, 160 + top, scale=TEXT_SCALE)
display.update()

# Calculate some offsets and swish each slice of the display back and forth
while True:
    n = math.pi / 1.5
    t = time.ticks_ms() / 500.0 / math.pi
    offset1 = WIDTH / 2 + math.sin(t) * offset
    offset2 = WIDTH / 2 + math.sin(t + n) * offset
    offset3 = WIDTH / 2 + math.sin(t + n + n) * offset
    display.set_scroll_group_offset(1, int(offset1), 0)
    display.set_scroll_group_offset(2, int(offset2), 0)
    display.set_scroll_group_offset(3, int(offset3), 0)
```

## Fill Rate & Drawing Things Fast

At stock clock on the CPU, PicoVision has a pixel fill rate of approximately 15 million pixels (16bit RGB555) a second.

This might sound significant, but that amounts to a screen clear at 720x480 taking 22ms.

That means that drawing is costly, and you should avoid uncessary drawing at all costs. While sprites are a great way to work around this, they're not always ideal.

* Use `set_clip` to restrict drawing to just the regions you want to change
* Use `rectangle` or `circle` instead of `clear` to clear regions for overdrawing
* Use scanline scrolling to move things around the screen
