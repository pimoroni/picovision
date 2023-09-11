# PicoVision: Python Documentation <!-- omit in toc -->

PicoVision uses a custom version of PicoGraphics that includes some new methods for working with the "GPU" features.

- [Sprites](#sprites)
  - [Loading Sprites](#loading-sprites)
- [Scanlines](#scanlines)
- [Frame vs Display](#frame-vs-display)


## Sprites

First, some terms-

* Image Index - an index to identify where sprite image data should be stored
* Sprite Slot - a single, hardware sprite. There are up to 32 (or 16 on widescreen versions).

### Loading Sprites

An index must be specified when loading an image, and can later be used to refer to that specific image and assign it to a sprite. It's a good idea to use a variable to keep track of these:

```python
IMAGE_ELEPHANT = 1
for _ in range(2):
    display.load_sprite("elephant.png", IMAGE_ELEPHANT)
    display.update()
```

Note: the quirk of loading the sprite twice in a loop is because we must swap the hardware PSRAM buffers and load the image data into each.

The `display_sprite` function serves both to display a sprite and to update its location or image:

```python
SPRITE_SLOT = 1
display.display_sprite(SPRITE_SLOT, IMAGE_ELEPHANT, X, Y, blend_mode=1, v_scale=1)
```

The sprite slot refers to a single "hardware" sprite. Each of these slots can display a specific image - referenced by its index - at a given X and Y position and with optional vertical scaling.

Once displayed the sprite will continue to display at its given X and Y coordinates without any intervention from your code. To remove it you need to clear it:

```
display.clear_sprite()

The current maximim size for a sprite is 2k, which corresponds to a 32x32 pixel image, however there are no restrictions on width/height so non-square images exceeding one of these dimensions and making up for it in the other is fine- so long as they don't exceed 2k.

## Scanlines

## Frame vs Display
