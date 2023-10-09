#pragma once

#include <cstring>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "common/pimoroni_common.hpp"
#include "common/pimoroni_i2c.hpp"
#include "drivers/aps6404/aps6404.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

#ifndef SUPPORT_WIDE_MODES
#define SUPPORT_WIDE_MODES 0
#endif

// Pimoroni DV Stick

namespace pimoroni {

  // This is ARGB1555 only for now
  class DVDisplay {
  public:
    static constexpr int PALETTE_SIZE = 32;
    static constexpr int NUM_PALETTES = 2;

#if SUPPORT_WIDE_MODES
    static constexpr int MAX_DISPLAYED_SPRITES = 32;
#else
    static constexpr int MAX_DISPLAYED_SPRITES = 80;
#endif

    enum Mode {
      MODE_PALETTE = 2,
      MODE_RGB555 = 1,
      MODE_RGB888 = 3,
    };

    enum Resolution : uint8_t {
        RESOLUTION_640x480 = 0,
        RESOLUTION_720x480 = 1,
        RESOLUTION_720x400 = 2,
        RESOLUTION_720x576 = 3,

        // These modes require the wide mode build
        RESOLUTION_800x600 = 0x10,
        RESOLUTION_800x480 = 0x11,
        RESOLUTION_800x450 = 0x12,
        RESOLUTION_960x540 = 0x13,
        RESOLUTION_960x540_50 = 0x14,
        RESOLUTION_1280x720 = 0x15,
    };

    enum SpriteBlendMode : uint8_t {
        BLEND_NONE = 0,     // Sprite data always written to frame regardless of alpha
        BLEND_DEPTH = 1,    // Alpha represents depth.  Order from back to front is: Sprite A0, Frame A0, Sprite A1, Frame A1
        BLEND_DEPTH2 = 2,   // Alpha represents depth.  Order from back to front is: Sprite A0, Frame A0, Frame A1, Sprite A1
                            // Note that blend modes are only supported in RGB555 mode.
        BLEND_BLEND = 3,    // Use frame if Sprite A0 or Frame A1, additive blend if Sprite A1 and Frame A0
        BLEND_BLEND2 = 4,   // Use frame if Sprite A0, additive blend if Sprite A1
    };

    // I2C pins
    static constexpr uint I2C_SDA = 6;
    static constexpr uint I2C_SCL = 7;

    // I2C address and registers
    static constexpr uint I2C_ADDR = 0x0D;
    static constexpr uint I2C_REG_SET_RES = 0xFC;
    static constexpr uint I2C_REG_START = 0xFD;
    static constexpr uint I2C_REG_STOP = 0xFF;
    static constexpr uint I2C_REG_GPIO = 0xC0;
    static constexpr uint I2C_REG_LED = 0xC1;
    static constexpr uint I2C_REG_GPIO29_MODE = 0xC2;
    static constexpr uint I2C_REG_GPIO29_OUT = 0xC3;
    static constexpr uint I2C_REG_GPIO29_ADC = 0xC4;
    static constexpr uint I2C_REG_GPU_TEMP = 0xC6;
    static constexpr uint I2C_REG_GPIO_HI = 0xC8;
    static constexpr uint I2C_REG_GPIO_HI_OUT = 0xC9;
    static constexpr uint I2C_REG_GPIO_HI_OE = 0xCA;
    static constexpr uint I2C_REG_GPIO_HI_PULL_UP = 0xCB;
    static constexpr uint I2C_REG_GPIO_HI_PULL_DOWN = 0xCC;
    static constexpr uint I2C_REG_EDID = 0xFB;
    static constexpr uint I2C_REG_PALETTE_INDEX = 0xF8;
    static constexpr uint I2C_REG_SCROLL_BASE = 0xE0;

    //--------------------------------------------------
    // Variables
    //--------------------------------------------------
  protected:
    friend void vsync_callback();

    // Ram accessed through the APS6404 driver
    APS6404 ram;

    // I2C interface to driver
    I2C* i2c;

    // interface pins
    static constexpr uint CS     = 17;
    static constexpr uint D0     = 19;
    static constexpr uint VSYNC  = 16;
    static constexpr uint RAM_SEL = 8;
    
    static constexpr uint32_t base_address = 0x10000;
    static constexpr uint32_t max_num_sprites = 1024;
    static constexpr uint32_t sprite_size = 0x900;
    static constexpr uint32_t sprite_base_address = 0x800000 - (max_num_sprites * sprite_size);
    uint16_t display_width = 0;
    uint16_t display_height = 0;
    uint16_t frame_width = 0;
    uint16_t frame_height = 0;
    uint8_t bank = 0;
    uint8_t h_repeat = 1;
    uint8_t v_repeat = 1;
    Mode mode = MODE_RGB555;

  public:
    // Valid resolutions are:
    //   640x480 (60Hz), 720x480 (60Hz), 720x400 (70Hz), 720x576 (50Hz)
    //   800x600 (60Hz), 800x480 (60Hz), 800x450 (60Hz), 960x540 (50Hz), 1280x720 (30Hz)
    // Note resolutions on the second line require quite extreme overclocking and may not work on all hardware.
    // Either or both of the horizontal or vertical component of any resolution may be halved.
    DVDisplay()
      : ram(CS, D0, pio1)
      , i2c(new I2C(I2C_SDA, I2C_SCL))
      , pixel_buffer_location(-1, -1)
    {}

    DVDisplay(I2C* i2c)
      : ram(CS, D0, pio1)
      , i2c(i2c)
      , pixel_buffer_location(-1, -1)
    {}

    //--------------------------------------------------
    // Methods
    //--------------------------------------------------
    public:
      static void preinit();
  
      void test(void){
        char writeBuffer[256];
        char readBuffer[256];

        uint mb = 8;

        for (uint b = 0; b < 2; ++b) {
          gpio_put(RAM_SEL, b);

          for(uint k = 0; k < 1024*mb; k++) {
            sprintf(writeBuffer, "%u-%u", b, k);

            ram.write(k*1024, (uint32_t *)writeBuffer, strlen(writeBuffer)+1);
          }
        }

        for (uint b = 0; b < 2; ++b) {
          gpio_put(RAM_SEL, b);
          bool bSame = true;
          for(uint k = 0; k < 1024*mb && bSame; k++)
          {
            sprintf(writeBuffer, "%u-%u", b, k);
            read(k*1024, strlen(writeBuffer)+1, (uint16_t *)readBuffer);
            bSame = strcmp(writeBuffer, readBuffer) == 0;
            printf("[%u] %s == %s ? %s\n", k, writeBuffer, readBuffer, bSame ? "Success" : "Failure");
          }
        }
      }
      
      // 16bpp interface
      void write_pixel(const Point &p, uint16_t colour);
      void write_pixel_span(const Point &p, uint l, uint16_t colour);
      void write_pixel_span(const Point &p, uint l, uint16_t *data);
      void read_pixel_span(const Point &p, uint l, uint16_t *data);

      // 24bpp interface
      void write_pixel(const Point &p, RGB888 colour);
      void write_pixel_span(const Point &p, uint l, RGB888 colour);
      void write_24bpp_pixel_span(const Point &p, uint len_in_pixels, uint8_t *data);
      void read_24bpp_pixel_span(const Point &p, uint len_in_pixels, uint8_t *data);

      bool init(uint16_t width, uint16_t height, Mode mode = MODE_RGB555, uint16_t frame_width = 0, uint16_t frame_height = 0);
      void flip();
      void reset();

      // flip_async queues a flip but returns without blocking.
      // You must call wait_for_flip before doing any more reads or writes, defining sprites, etc.
      void flip_async();
      void wait_for_flip();

      // Define the data for a sprite, the specified data index can then be supplied
      // to set_sprite to use the sprite.  Up to 1024 sprites can be defined.
      // Each sprite can be up to 2KB big, with a maximum width or height of 64 pixels.
      // So for ARGB1555 sprites 64x16, 32x32, 16x64 are examples of maximum sizes.
      // You must define the sprite to each RAM bank.
      void define_sprite(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint16_t* data);

      // Palette mode sprites are defined with the colour number in bits 6-2 of each byte and alpha in bit 0.
      // Because palette mode sprites only use 1 byte per pixel they can be bigger, up to 64x32, or 45x45, for example.
      void define_palette_sprite(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint8_t* data);

      // Load a sprite stored in PicoVision sprite format
      void load_pvs_sprite(uint16_t sprite_data_idx, uint32_t* data, uint32_t len_in_bytes);

      // Display/move a sprite to a given position.
      // Note sprite positions are always display relative (not scrolled)
      void set_sprite(int sprite_num, uint16_t sprite_data_idx, const Point &p, SpriteBlendMode blend_mode = BLEND_DEPTH, int v_scale = 1);
      void clear_sprite(int sprite_num);

      // 32 colour palette mode.  Note that palette entries range from 0-31,
      // but when writing colour values the palette entry is in bits 6-2, so the
      // entry value is effectively multiplied by 4.
      // The palette idx 
      void set_mode(Mode new_mode);
      void set_palette(RGB888 palette[PALETTE_SIZE], int palette_idx = 0);
      void set_palette(RGB888 palette[PALETTE_SIZE]);
      void set_palette_colour(uint8_t entry, RGB888 colour, int palette_idx);
      void set_palette_colour(uint8_t entry, RGB888 colour);
      void set_display_palette_index(uint8_t idx);
      void set_local_palette_index(uint8_t idx);
      uint8_t get_local_palette_index();
      RGB888* get_palette(uint8_t idx = 0);

      void write_palette_pixel(const Point &p, uint8_t colour);
      void write_palette_pixel_span(const Point &p, uint l, uint8_t colour);
      void write_palette_pixel_span(const Point &p, uint l, uint8_t* data);
      void read_palette_pixel_span(const Point &p, uint l, uint8_t *data);

      // Set the scroll offset and wrap for a set of scanlines on the display.  There are 7 scroll offsets, indexes 1-7.
      // By default, all scanlines are offset by scroll idx 1, so setting this effectively moves the 
      // top left corner of the display within the frame.
      // When reading across the frame, the display will skip from `from_x` to `to_x`.
      void setup_scroll_group(const Point& p, int idx=1, int wrap_from_x=0, int wrap_from_y=0, int wrap_to_x=0, int wrap_to_y=0);

      // Configure the scroll offset index to use for a set of scanlines (inclusive of miny, exclusive of maxy),
      // this applies to the current bank only - you need to set again after flipping to apply the same setting to the other bank.
      void set_scroll_idx_for_lines(int idx, int miny, int maxy);

      // Returns the state of GPU GPIOs 23-29 as a bitfield.
      uint8_t get_gpio();

      bool get_gpio_29() { return (get_gpio() & 0x40) == 0x40; }
      void set_gpio_29_dir(bool output);

      // Set the proportion of the PWM cycle that pin 29 is on for, this is linear in the range 0-255.
      void set_gpio_29_value(uint8_t pwm_value);
      void set_gpio_29(bool on) { on ? set_gpio_29_value(255) : set_gpio_29_value(0); }

      // Pin 29 must be set as an input before setting pull up or down.
      // The pull configuration is lost if the pin mode is changed.
      void set_gpio_29_pull_up(bool on);
      void set_gpio_29_pull_down(bool on);

      // ADC on pin 29.  Enabling the ADC disables input and output
      // Setting the direction on pin 29 disables the ADC.
      void enable_gpio_29_adc();
      float get_gpio_29_adc();  // in V

      // The "Hi" GPIO are marked: CK, CS, D0, D1, D2, D3, D+, D-
      // Returns the state of the Hi GPIOs as a bitfield.
      uint8_t get_gpio_hi();
      bool get_gpio_hi(uint8_t pin) { return (get_gpio_hi() & (1 << pin)) != 0; }
      void set_gpio_hi_dir(uint pin, bool output);
      void set_gpio_hi_dir_all(uint8_t output_enables);
      void set_gpio_hi(uint pin, bool on);
      void set_gpio_hi_all(uint8_t vals);
      void set_gpio_hi_pull_up(uint pin, bool on);
      void set_gpio_hi_pull_up_all(uint8_t vals);
      void set_gpio_hi_pull_down(uint pin, bool on);
      void set_gpio_hi_pull_down_all(uint8_t vals);
      float get_gpu_temp();  // in C

      bool is_button_x_pressed() { return (get_gpio() & 0x1) != 0x1; }
      bool is_button_a_pressed() { return (get_gpio() & 0x2) != 0x2; }

      // Valid LED levels are from 0-127.
      void set_led_level(uint8_t level);
      void set_led_level(float level) { set_led_level((uint8_t)(level * 127.f)); }
      void set_led_heartbeat();

      // The supplied buffer must be at least 128 bytes long
      void get_edid(uint8_t* edid);

      // Raw access to frame buffer
      uint32_t point_to_address(const Point& p) const;
      int pixel_size() const;
      int frame_row_stride() const { return (int)frame_width * 3; }
      void raw_read_async(uint32_t address, uint32_t* data, uint32_t len_in_words) { ram.read(address, data, len_in_words); }
      void raw_write_async(uint32_t address, uint32_t* data, uint32_t len_in_words) { ram.write(address, data, len_in_words << 2); }
      void raw_write_async_bytes(uint32_t address, uint32_t* data, uint32_t len_in_bytes) { ram.write(address, data, len_in_bytes); }
      void raw_wait_for_finish_blocking() { ram.wait_for_finish_blocking(); }

    protected:
      uint8_t palette[NUM_PALETTES * PALETTE_SIZE * 3] alignas(4);
      uint8_t change_mode = 0;
      uint8_t rewrite_header = 0;
      uint8_t rewrite_palette = 0;
      uint8_t current_palette = 0;

      virtual void write_palette();
      virtual void write_header();
      virtual void write_sprite_table();
      virtual void write_header_preamble();

      void i2c_modify_bit(uint8_t reg, uint bit, bool enable);

    private:
      static constexpr int PIXEL_BUFFER_LEN_IN_WORDS = 32;
      uint32_t pixel_buffer[PIXEL_BUFFER_LEN_IN_WORDS];
      Point pixel_buffer_location;
      int32_t pixel_buffer_x;

      void write(uint32_t address, size_t len, const uint16_t colour);
      void read(uint32_t address, size_t len, uint16_t *data);
      void write(uint32_t address, size_t len, const uint8_t colour);
      void read(uint32_t address, size_t len, uint8_t *data);
      void write(uint32_t address, size_t len, const RGB888 colour);

      void define_sprite_internal(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint32_t* data, uint32_t bytes_per_pixel);

      uint32_t point_to_address16(const Point &p) const {
        return base_address + (p.y * (uint32_t)frame_width * 3) + (p.x * 2);
      }

      uint32_t point_to_address_palette(const Point &p) const {
        return base_address + (p.y * (uint32_t)frame_width * 3) + p.x;
      }

      uint32_t point_to_address24(const Point &p) const {
        return base_address + ((p.y * (uint32_t)frame_width) + p.x) * 3;
      }
  };
}
