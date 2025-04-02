#include "dv_display.hpp"
#include "swd_load.hpp"
#if SUPPORT_WIDE_MODES
#include "pico-stick-wide.h"
#else
#include "pico-stick.h"
#endif

#include <cstdlib>
#include <math.h>
#include <string.h>
#ifndef MICROPY_BUILD_TYPE
#define mp_printf(_, ...) printf(__VA_ARGS__);
#define my_thread_yield()
#else
extern "C" {
#include "py/runtime.h"

// Must be a C function
static void my_thread_yield() {
  MICROPY_EVENT_POLL_HOOK
}
}
#endif

namespace {
  template <typename T>
  bool is_transparent(T val);
  
  template <>
  bool is_transparent(uint8_t val) {
    return (val & 1) == 0;
  }

  template <>
  bool is_transparent(uint16_t val) {
    return (val & 0x8000) == 0;
  }

  template<typename T>
  void get_sprite_limits(T* data, uint16_t width, uint8_t& offset, uint8_t& length)
  {
    offset = 0;
    length = 0;
    
    for (uint16_t i = 0; i < width; ++i, ++data) {
      if (offset == i) {
        if (is_transparent(*data)) ++offset;
        else length = 1;
      }
      else if (!is_transparent(*data)) {
        length = i + 1 - offset;
      }
    }
  }
}

namespace pimoroni {
  volatile bool enable_switch_on_vsync = false;

  void vsync_callback() {
    if (gpio_get_irq_event_mask(DVDisplay::VSYNC) & GPIO_IRQ_EDGE_RISE) {
      gpio_acknowledge_irq(DVDisplay::VSYNC, GPIO_IRQ_EDGE_RISE);

      if (enable_switch_on_vsync) {
        // Toggle RAM_SEL pin
        gpio_xor_mask(1 << DVDisplay::RAM_SEL);
        enable_switch_on_vsync = false;
      }
    }
  }

  void DVDisplay::preinit() {
    gpio_init(RAM_SEL);
    gpio_put(RAM_SEL, 0);
    gpio_set_dir(RAM_SEL, GPIO_OUT);

    gpio_init(VSYNC);
    gpio_set_dir(VSYNC, GPIO_IN);

    swd_load_program(section_addresses, section_data, section_data_len, sizeof(section_addresses) / sizeof(section_addresses[0]), 0x20000001, 0x15004000, true);

#ifndef MICROPY_BUILD_TYPE
      // We set a high priority shared handler on the IO_IRQ_BANK0 interrupt, instead
      // of using the pico-sdk methods for GPIO interrupts, because MicroPython sets up its
      // own shared handler replacing the SDK method.  By setting our handler high priority
      // we get in before either the pico-sdk or Micropython handler, so it works in both cases.
      gpio_set_irq_enabled(VSYNC, GPIO_IRQ_EDGE_RISE, true);
      irq_add_shared_handler(IO_IRQ_BANK0, vsync_callback, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY + 1);
      irq_set_enabled(IO_IRQ_BANK0, true);
#endif
  };

  bool DVDisplay::init(uint16_t display_width_, uint16_t display_height_, Mode mode_, uint16_t frame_width_, uint16_t frame_height_, bool maximum_compatibility) {
    display_width = display_width_;
    display_height = display_height_;

    if (frame_width_ == 0) frame_width = display_width_;
    else frame_width = frame_width_;

    // Internal frame width should be a multiple of 4.
    if (frame_width & 3) frame_width += 4 - (frame_width & 3);

    if (frame_height_ == 0) frame_height = display_height_;
    else frame_height = frame_height_;

    bank = 0;
    h_repeat = 1;
    v_repeat = 1;

    uint8_t res_mode = 0xFF;
    uint16_t full_width = display_width;
    uint16_t full_height = display_height;

    if (display_width < 640 || (display_width == 640 && (display_height == 360 || display_height == 720))) {
      h_repeat = 2;
      full_width *= 2;
    }

    if (mode_ == MODE_RGB888 && h_repeat == 1) {
      // Mode is unsupported
      return false;
    }

    if (display_height < 400) {
      v_repeat = 2;
      full_height *= 2;
    }

    if (full_width == 640) {
      res_mode = RESOLUTION_640x480;
    }
    else if (full_width == 720) {
      if (full_height == 480) res_mode = RESOLUTION_720x480;
      else if (full_height == 400) res_mode = RESOLUTION_720x400;
      else if (full_height == 576) res_mode = RESOLUTION_720x576;
    }
#if SUPPORT_WIDE_MODES
    // Modes below require the widescreen build
    else if (full_width == 800) {
      if (full_height == 600) res_mode = RESOLUTION_800x600;
      else if (full_height == 480) res_mode = RESOLUTION_800x480;
      else if (full_height == 450) res_mode = RESOLUTION_800x450;
    }
    else if (full_width == 960) {
      if (full_height == 540) res_mode = RESOLUTION_960x540_50;
    }
    else if (full_width == 1280) {
      if (full_height == 720) res_mode = RESOLUTION_1280x720;
    }
#endif

    if (res_mode == 0xFF) {
      // Resolution is unsupported
      return false;
    }

    gpio_put(RAM_SEL, 0);
    ram.init();
    bank = 0;
    write_header();
    sleep_us(100);

    gpio_put(RAM_SEL, 1);
    ram.init();
    bank = 1;
    write_header();
    sleep_us(100);

    bank = 0;
    gpio_put(RAM_SEL, 0);
    sleep_ms(50);

    mp_printf(&mp_plat_print, "Start I2C\n");

    if (res_mode != 0xFF) {
      if (maximum_compatibility) res_mode |= 0x80;
      i2c->reg_write_uint8(I2C_ADDR, I2C_REG_SET_RES, res_mode);
    }

    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_START, 1);
    mp_printf(&mp_plat_print, "Started\n");

    set_mode(mode_);

#ifdef MICROPY_BUILD_TYPE
      // We set a high priority shared handler on the IO_IRQ_BANK0 interrupt, instead
      // of using the pico-sdk methods for GPIO interrupts, because MicroPython sets up its
      // own shared handler replacing the SDK method.  By setting our handler high priority
      // we get in before either the pico-sdk or Micropython handler, so it works in both cases.
      gpio_set_irq_enabled(VSYNC, GPIO_IRQ_EDGE_RISE, true);
      irq_add_shared_handler(IO_IRQ_BANK0, vsync_callback, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY + 1);
      irq_set_enabled(IO_IRQ_BANK0, true);
#endif

    return true;
  }
  
  void DVDisplay::flip_async() {
    if (mode == MODE_PALETTE) {
      if (rewrite_palette > 0) {
        write_palette();
        --rewrite_palette;
      }
      if (pixel_buffer_location.y != -1) {
        ram.write(point_to_address_palette(pixel_buffer_location), pixel_buffer, pixel_buffer_x);
        pixel_buffer_location.y = -1;
      }
    }
    else if (pixel_buffer_location.y != -1) {
      ram.write(point_to_address16(pixel_buffer_location), pixel_buffer, pixel_buffer_x << 1);
      pixel_buffer_location.y = -1;
    }

    if (rewrite_header) {
      set_scroll_idx_for_lines(-1, 0, display_height);
      write_sprite_table();
      --rewrite_header;
    }

    bank ^= 1;
    ram.wait_for_finish_blocking();

    enable_switch_on_vsync = true;

    if (change_mode) {
      wait_for_flip();
    }
  }

  void DVDisplay::flip() {
    flip_async();
    wait_for_flip();
  }

  void DVDisplay::wait_for_flip() {
    while (enable_switch_on_vsync) {
      // Waiting for IRQ handler to do flip.
      my_thread_yield();
    }

    if (change_mode) {
      --change_mode;
      write_sprite_table();
      set_scroll_idx_for_lines(0, 0, display_height);
    }
  }

  void DVDisplay::reset() {
    for(auto i = 0u; i < MAX_DISPLAYED_SPRITES; i++) {
      clear_sprite(i);
    }
    //swd_reset();
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_STOP, 1);
#ifdef MICROPY_BUILD_TYPE
    irq_remove_handler(IO_IRQ_BANK0, vsync_callback);
#endif
  }

  void DVDisplay::setup_scroll_group(const Point& p, int idx, int wrap_from_x, int wrap_from_y, int wrap_to_x, int wrap_to_y) {
    if (idx < 1 || idx > 7) {
      return;
    }

    int16_t wrap_position = 0;
    int16_t wrap_offset = 0;
    if (wrap_from_x > p.x && wrap_from_x < p.x + display_width) {
      wrap_position = (wrap_from_x - p.x) * pixel_size();
      wrap_offset = (wrap_to_x - wrap_from_x) * pixel_size();
    }

    int32_t addr_offset = (int32_t)point_to_address(p) - (int32_t)point_to_address({0,0});
    int32_t addr_offset2 = (int32_t)point_to_address({p.x, p.y+wrap_to_y}) - (int32_t)point_to_address({0,wrap_from_y});
    uint32_t max_addr = 0;
    if (wrap_from_y > p.y && wrap_from_y < p.y + display_height) {
      max_addr = point_to_address({0, wrap_from_y});
    }

    uint8_t scroll_config[13];
    scroll_config[0] = addr_offset & 0xFF;
    scroll_config[1] = (addr_offset >> 8) & 0xFF;
    scroll_config[2] = (addr_offset >> 16) & 0xFF;
    scroll_config[3] = max_addr & 0xFF;
    scroll_config[4] = (max_addr >> 8) & 0xFF;
    scroll_config[5] = (max_addr >> 16) & 0xFF;
    scroll_config[6] = addr_offset2 & 0xFF;
    scroll_config[7] = (addr_offset2 >> 8) & 0xFF;
    scroll_config[8] = (addr_offset2 >> 16) & 0xFF;
    scroll_config[9] = wrap_position & 0xFF;
    scroll_config[10] = (wrap_position >> 8) & 0xFF;
    scroll_config[11] = wrap_offset & 0xFF;
    scroll_config[12] = (wrap_offset >> 8) & 0xFF;

    i2c->write_bytes(I2C_ADDR, I2C_REG_SCROLL_BASE + idx, scroll_config, 13);
  }

  void DVDisplay::set_scroll_idx_for_lines(int idx, int miny, int maxy) {
    constexpr int buf_size = 32;
    uint32_t buf[buf_size];
    uint addr = 4 * (7 + miny);
    uint line_type = (uint)mode << 27;
    if (idx >= 0) line_type |= (uint)idx << 29;
    for (int i = miny; i < maxy; i += buf_size) {
      int maxj = std::min(buf_size, maxy - i);
      if (idx >= 0) {
        for (int j = 0; j < maxj; ++j) {
          buf[j] = line_type + ((uint32_t)h_repeat << 24) + ((i + j) * frame_width * 3) + base_address;
        }
      }
      else {
        ram.read_blocking(addr, buf, maxj);
        for (int j = 0; j < maxj; ++j) {
          buf[j] &= 0xE0000000;
          buf[j] |= line_type + ((uint32_t)h_repeat << 24) + ((i + j) * frame_width * 3) + base_address;
        }
      }
      ram.write(addr, buf, maxj * 4);
      ram.wait_for_finish_blocking();
      addr += 4 * maxj;
    }
  }

  uint8_t DVDisplay::get_gpio() {
    return i2c->reg_read_uint8(I2C_ADDR, I2C_REG_GPIO);
  }

  void DVDisplay::set_gpio_29_dir(bool output) {
    uint8_t pin29_mode = i2c->reg_read_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE);
    if (output) {
      pin29_mode = 5;
    }
    else if (pin29_mode > 3) {
      // Pin is not an input, set to input with no pulls
      pin29_mode = 0;
    }
    else {
      // Pin is already an input, don't change the pulls
      return;
    }
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE, pin29_mode);
  }

  void DVDisplay::set_gpio_29_value(uint8_t pwm_value) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO29_OUT, pwm_value);
  }

  void DVDisplay::set_gpio_29_pull_up(bool on) {
    uint8_t pin29_mode = i2c->reg_read_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE);
    if (pin29_mode > 3) {
      return;
    }
    if (on) pin29_mode |= 1;
    else pin29_mode &= ~1;
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE, pin29_mode);
  }

  void DVDisplay::set_gpio_29_pull_down(bool on) {
    uint8_t pin29_mode = i2c->reg_read_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE);
    if (pin29_mode > 3) {
      return;
    }
    if (on) pin29_mode |= 2;
    else pin29_mode &= ~2;
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE, pin29_mode);
  }

  void DVDisplay::enable_gpio_29_adc() {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO29_MODE, 6);
  }

  float DVDisplay::get_gpio_29_adc() {
    constexpr float conversion = 3.3f / (1 << 12);
    return i2c->reg_read_uint16(I2C_ADDR, I2C_REG_GPIO29_ADC) * conversion;
  }

  uint8_t DVDisplay::get_gpio_hi() {
    return i2c->reg_read_uint8(I2C_ADDR, I2C_REG_GPIO_HI);
  }

  void DVDisplay::i2c_modify_bit(uint8_t reg, uint bit, bool enable) {
    uint8_t val = i2c->reg_read_uint8(I2C_ADDR, reg);
    if (enable) val |= 1u << bit;
    else val &= ~(1u << bit);
    i2c->reg_write_uint8(I2C_ADDR, reg, val);
  }

  void DVDisplay::set_gpio_hi_dir(uint pin, bool output) {
    i2c_modify_bit(I2C_REG_GPIO_HI_OE, pin, output);
  }

  void DVDisplay::set_gpio_hi_dir_all(uint8_t val) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO_HI_OE, val);
  }

  void DVDisplay::set_gpio_hi(uint pin, bool on) {
    i2c_modify_bit(I2C_REG_GPIO_HI_OUT, pin, on);
  }

  void DVDisplay::set_gpio_hi_all(uint8_t val) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO_HI_OUT, val);
  }

  void DVDisplay::set_gpio_hi_pull_up(uint pin, bool on) {
    i2c_modify_bit(I2C_REG_GPIO_HI_PULL_UP, pin, on);
  }

  void DVDisplay::set_gpio_hi_pull_up_all(uint8_t val) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO_HI_PULL_UP, val);
  }

  void DVDisplay::set_gpio_hi_pull_down(uint pin, bool on) {
    i2c_modify_bit(I2C_REG_GPIO_HI_PULL_DOWN, pin, on);
  }

  void DVDisplay::set_gpio_hi_pull_down_all(uint8_t val) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_GPIO_HI_PULL_DOWN, val);
  }

  float DVDisplay::get_gpu_temp() {
    uint16_t raw_value = i2c->reg_read_uint16(I2C_ADDR, I2C_REG_GPU_TEMP);

    constexpr float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)raw_value * conversionFactor;
    return 27.0f - (adc - 0.706f) / 0.001721f;
  }

  void DVDisplay::set_led_level(uint8_t level) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_LED, level | 0x80);
  }

  void DVDisplay::set_led_heartbeat() {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_LED, 2);
  }

  void DVDisplay::get_edid(uint8_t* edid) {
    i2c->read_bytes(I2C_ADDR, I2C_REG_EDID, edid, 128);
  }

  void DVDisplay::set_display_palette_index(uint8_t idx) {
    i2c->reg_write_uint8(I2C_ADDR, I2C_REG_PALETTE_INDEX, idx);
  }

  void DVDisplay::set_local_palette_index(uint8_t idx) {
    current_palette = idx;
  }

  uint8_t DVDisplay::get_local_palette_index() {
    return current_palette;
  }

  void DVDisplay::write(uint32_t address, size_t len, const uint16_t colour)
  {
    uint32_t val = colour | ((uint32_t)colour << 16);

    ram.write_repeat(address, val, len << 1);
  }

  void DVDisplay::write(uint32_t address, size_t len, const RGB888 colour)
  {
    const int VAL_BUFFER_LEN = 60;
    const int VAL_BUFFER_LEN_IN_PIXELS = (VAL_BUFFER_LEN / 3) * 4;
    uint32_t vals[VAL_BUFFER_LEN];

    for (int i = 0, j = 0; i < VAL_BUFFER_LEN_IN_PIXELS && i < (int)len; i += 4) {
      vals[j++] = colour | (colour << 24);
      vals[j++] = (colour >> 8) | (colour << 16);
      vals[j++] = (colour >> 16) | (colour << 8);
    }

    for (int len_bytes = len * 3; len_bytes > 0; len_bytes -= VAL_BUFFER_LEN_IN_PIXELS * 3) {
      uint len_to_write = std::min(len_bytes, VAL_BUFFER_LEN_IN_PIXELS * 3);
      ram.write(address, vals, len_to_write);
      address += len_to_write;
    }
    ram.wait_for_finish_blocking();
  }

  void DVDisplay::read(uint32_t address, size_t len, uint16_t *data)
  {
    if ((uintptr_t)data & 1) {
      uint32_t tmp;
      ram.read_blocking(address, &tmp, 1);
      *data++ = tmp & 0xFFFF;
      len--;
    }
    if ((len >> 1) > 0) {
      ram.read_blocking(address, (uint32_t*)data, len >> 1);
    }
    if (len & 1) {
      uint32_t tmp;
      ram.read_blocking(address, &tmp, 1);
      data[len-1] = tmp & 0xFFFF;
    }
  }

  void DVDisplay::write(uint32_t address, size_t len, const uint8_t colour)
  {
    uint32_t val = colour | ((uint32_t)colour << 16);
    val |= val << 8;

    ram.write_repeat(address, val, len);
  }

  void DVDisplay::read(uint32_t address, size_t len, uint8_t *data)
  {
    if ((uintptr_t)data & 3) {
      uint32_t tmp;
      int unaligned_len = std::min(4 - ((uintptr_t)data & 3), len);
      ram.read_blocking(address, &tmp, 1);
      for (int i = 0; i < unaligned_len; ++i) {
        *data++ = tmp & 0xFF;
        tmp >>= 8;
      }
      len -= unaligned_len;
    }
    if ((len >> 2) > 0) {
      ram.read_blocking(address, (uint32_t*)data, len >> 2);
    }
    if (len & 3) {
      uint32_t tmp;
      ram.read_blocking(address, &tmp, 1);
      data += len & ~3;
      for (uint32_t i = 0; i < (len & 3); ++i) {
        *data++ = tmp & 0xFF;
        tmp >>= 8;
      }
    }
  }

  void DVDisplay::write_pixel(const Point &p, uint16_t colour)
  {
    if (pixel_buffer_location.y == p.y && pixel_buffer_location.x + pixel_buffer_x == p.x) {
      if (pixel_buffer_x & 1) pixel_buffer[pixel_buffer_x >> 1] |= (uint32_t)colour << 16;
      else pixel_buffer[pixel_buffer_x >> 1] = colour;
      if (++pixel_buffer_x == PIXEL_BUFFER_LEN_IN_WORDS * 2) {
        ram.write(point_to_address16(pixel_buffer_location), pixel_buffer, PIXEL_BUFFER_LEN_IN_WORDS * 4);
        pixel_buffer_location.y = -1;
      }
      return;
    }
    else if (pixel_buffer_location.y != -1) {
      ram.write(point_to_address16(pixel_buffer_location), pixel_buffer, pixel_buffer_x << 1);
    }
    pixel_buffer_location = p;
    pixel_buffer_x = 1;
    pixel_buffer[0] = colour;
  }

  void DVDisplay::write_pixel_span(const Point &p, uint l, uint16_t colour)
  {
    write(point_to_address16(p), l, colour);
  }

  void DVDisplay::write_pixel(const Point &p, RGB888 colour)
  {
    ram.write(point_to_address24(p), &colour, 3);
  }

  void DVDisplay::write_pixel_span(const Point &p, uint l, RGB888 colour)
  {
    write(point_to_address24(p), l, colour);
  }

  void DVDisplay::write_pixel_span(const Point &p, uint l, uint16_t *data)
  {
    uint32_t offset = 0;
    if (((uintptr_t)data & 0x2) != 0) {
      uint32_t val = *data++;
      ram.write(point_to_address16(p), &val, 2);
      --l;
      offset = 2;
    }
    if (l > 0) {
      ram.write(point_to_address16(p) + offset, (uint32_t*)data, l << 1);
    }
  }

  void DVDisplay::read_pixel_span(const Point &p, uint l, uint16_t *data)
  {
    read(point_to_address16(p), l, data);
  }

  void DVDisplay::set_mode(Mode new_mode)
  {
    if (new_mode == MODE_RGB888 && h_repeat == 1) {
      // Don't allow mode switch to non pixel doubled RGB888
      return;
    }

    mode = new_mode;
    set_scroll_idx_for_lines(0, 0, display_height);
    write_sprite_table();
    for (int i = 0; i < MAX_DISPLAYED_SPRITES; ++i) {
      clear_sprite(i);
    }
    change_mode = 1;
    if (mode == MODE_PALETTE) {
      rewrite_palette = 2;
    }
  }
  
  void DVDisplay::write_24bpp_pixel_span(const Point &p, uint len_in_pixels, uint8_t *data)
  {
    uint32_t offset = 0;
    uint32_t address = point_to_address24(p);
    uint l = len_in_pixels * 3;
    if (((uintptr_t)data & 0x3) != 0 && l > 0) {
      uint32_t val = data[0] | (data[1] << 8) | (data[2] << 16);
      offset = 4 - ((uintptr_t)data & 0x3);
      ram.write(address, &val, offset);
      l -= offset;
      address += offset;
      ram.wait_for_finish_blocking();
    }
    if (l > 0) {
      ram.write(address, (uint32_t*)(data + offset), l);
    }
  }

  void DVDisplay::read_24bpp_pixel_span(const Point &p, uint len_in_pixels, uint8_t *data)
  {
    read(point_to_address24(p), len_in_pixels * 3, data);
  }

  void DVDisplay::set_palette(RGB888 new_palette[PALETTE_SIZE], int palette_idx)
  {
    for (int i = 0; i < PALETTE_SIZE; ++i) {
      set_palette_colour(i, new_palette[i], palette_idx);
    }
  }

  void DVDisplay::set_palette_colour(uint8_t entry, RGB888 colour, int palette_idx)
  {
    uint8_t* palette_entry = palette + (palette_idx * PALETTE_SIZE + entry) * 3;
    palette_entry[0] = (colour >> 16) & 0xFF;
    palette_entry[1] = (colour >> 8) & 0xFF;
    palette_entry[2] = colour & 0xFF;
    rewrite_palette = 2;
  }

  void DVDisplay::set_palette(RGB888 new_palette[PALETTE_SIZE])
  {
    set_palette(new_palette, current_palette);
  }

  void DVDisplay::set_palette_colour(uint8_t entry, RGB888 colour)
  {
    set_palette_colour(entry, colour, current_palette);
  }

  void DVDisplay::write_palette()
  {
    uint addr = (display_height + 7) * 4;
    ram.write(addr, (uint32_t*)palette, NUM_PALETTES * PALETTE_SIZE * 3);
  }

  RGB888* DVDisplay::get_palette(uint8_t idx) {
    return (uint32_t*)palette;
  }

  void DVDisplay::write_palette_pixel(const Point &p, uint8_t colour)
  {
    if (pixel_buffer_location.y == p.y && pixel_buffer_location.x + pixel_buffer_x == p.x) {
      if (pixel_buffer_x & 3) pixel_buffer[pixel_buffer_x >> 2] |= (uint32_t)colour << ((pixel_buffer_x & 3) << 3);
      else pixel_buffer[pixel_buffer_x >> 2] = colour;
      if (++pixel_buffer_x == PIXEL_BUFFER_LEN_IN_WORDS * 4) {
        ram.write(point_to_address_palette(pixel_buffer_location), pixel_buffer, PIXEL_BUFFER_LEN_IN_WORDS * 4);
        pixel_buffer_location.y = -1;
      }
      return;
    }
    else if (pixel_buffer_location.y != -1) {
      ram.write(point_to_address_palette(pixel_buffer_location), pixel_buffer, pixel_buffer_x);
    }
    pixel_buffer_location = p;
    pixel_buffer_x = 1;
    pixel_buffer[0] = colour;
  }
  
  void DVDisplay::write_palette_pixel_span(const Point &p, uint l, uint8_t colour)
  {
    write(point_to_address_palette(p), l, colour);
  }
  
  void DVDisplay::write_palette_pixel_span(const Point &p, uint l, uint8_t* data)
  {
    uint offset = 0;
    uint32_t address = point_to_address_palette(p);
    if (((uintptr_t)data & 0x3) != 0 && l > 0) {
      uint32_t val = data[0] | (data[1] << 8) | (data[2] << 16);
      offset = 4 - ((uintptr_t)data & 0x3);
      ram.write(address, &val, std::min(offset, l));
      l -= offset;
      address += offset;
      ram.wait_for_finish_blocking();
    }
    if (l > 0) {
      ram.write(address, (uint32_t*)(data + offset), l);
    }
  }
  
  void DVDisplay::read_palette_pixel_span(const Point &p, uint l, uint8_t *data)
  {
    read(point_to_address_palette(p), l, data);
  }

  void DVDisplay::write_header_preamble()
  {
    uint32_t buf[8];
    uint32_t full_width = display_width * h_repeat;
    buf[0] = 0x4F434950;
    buf[1] = 0x01000101 + ((uint32_t)v_repeat << 16);
    buf[2] = full_width << 16;
    buf[3] = (uint32_t)display_height << 16;
    buf[4] = 0x00000001;
    buf[5] = 0x00000000 + display_height + ((uint32_t)bank << 24);
    buf[6] = 0x04000000 + NUM_PALETTES;
    ram.write(0, buf, 7 * 4);
    ram.wait_for_finish_blocking();
  }

  void DVDisplay::write_sprite_table()
  {
    constexpr uint32_t buf_size = 32;
    uint32_t buf[buf_size];

    uint addr = (display_height + 7) * 4 + NUM_PALETTES * PALETTE_SIZE * 3;
    uint sprite_type = (uint)mode << 28;
    for (uint32_t i = 0; i < max_num_sprites; i += buf_size) {
      for (uint32_t j = 0; j < buf_size; ++j) {
        buf[j] = sprite_type + (i + j) * sprite_size + sprite_base_address;
      }
      ram.write(addr, buf, buf_size * 4);
      ram.wait_for_finish_blocking();
      addr += buf_size * 4;
    }
  }

  void DVDisplay::write_header()
  {
    write_header_preamble();
    set_scroll_idx_for_lines(0, 0, display_height);
    write_sprite_table();
  }

  void DVDisplay::define_sprite_internal(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint32_t* data, uint32_t bytes_per_pixel)
  {
    // Work out the non-transparent part of each line to
    // minimize the size of the sprite data the GPU needs to load
    uint8_t offsets[32];
    uint8_t lengths[32];

    if (bytes_per_pixel == 1) {
      uint8_t *byte_data = (uint8_t*)data;
      for (uint16_t i = 0; i < height; ++i)
      {
        get_sprite_limits(byte_data, width, offsets[i], lengths[i]);
        byte_data += width;
      }    
    }
    else {
      uint16_t *word_data = (uint16_t*)data;
      for (uint16_t i = 0; i < height; ++i)
      {
        get_sprite_limits(word_data, width, offsets[i], lengths[i]);
        word_data += width;
      }
    }

    // Clip off the bottom of the image if it is empty
    // Don't clip the top because that controls where the user positions the sprite.
    while (height > 1 && lengths[height-1] == 0) --height;

    uint32_t buf[33];
    uint16_t* buf_ptr = (uint16_t*)buf;
    uint addr = sprite_base_address + sprite_data_idx * sprite_size;
    
    *buf_ptr++ = (height << 8) + width;

    for (uint16_t i = 0; i < height; ++i)
    {
      *buf_ptr++ = (lengths[i] << 8) | offsets[i];
    }
    uint len = (uint8_t*)buf_ptr - (uint8_t*)buf;
    ram.write(addr, buf, len);

    addr += len;
    if (len & 2) addr += 2;

    for (uint16_t i = 0; i < height; ++i)
    {
      uint8_t* byte_data = (uint8_t*)data + (width * i + offsets[i]) * bytes_per_pixel;
      if ((uintptr_t)byte_data & 3) {
        // Can only write from an aligned buffer
        ram.wait_for_finish_blocking();
        memcpy(buf, byte_data, lengths[i] * bytes_per_pixel);
        ram.write(addr, buf, lengths[i] * bytes_per_pixel);
      }
      else {
        ram.write(addr, (uint32_t*)byte_data, lengths[i] * bytes_per_pixel);
      }
      addr += lengths[i] * bytes_per_pixel;
    }
    ram.wait_for_finish_blocking();
  }

  void DVDisplay::define_sprite(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint16_t* data)
  {
    define_sprite_internal(sprite_data_idx, width, height, (uint32_t*)data, 2);
  }

  void DVDisplay::define_palette_sprite(uint16_t sprite_data_idx, uint16_t width, uint16_t height, uint8_t* data)
  {
    define_sprite_internal(sprite_data_idx, width, height, (uint32_t*)data, 1);
  }

  void DVDisplay::load_pvs_sprite(uint16_t sprite_data_idx, uint32_t* data, uint32_t len_in_bytes)
  {
    uint addr = sprite_base_address + sprite_data_idx * sprite_size;
    ram.write(addr, data, len_in_bytes);
    ram.wait_for_finish_blocking();
  }

  void DVDisplay::set_sprite(int sprite_num, uint16_t sprite_data_idx, const Point &p, SpriteBlendMode blend_mode, int v_scale)
  {
    if (sprite_num < MAX_DISPLAYED_SPRITES) {
      uint8_t buf[7];
      buf[0] = (uint8_t)blend_mode | ((v_scale - 1) << 3);
      buf[1] = sprite_data_idx & 0xff;
      buf[2] = sprite_data_idx >> 8;
      buf[3] = p.x & 0xff;
      buf[4] = p.x >> 8;
      buf[5] = p.y & 0xff;
      buf[6] = p.y >> 8;

      i2c->write_bytes(I2C_ADDR, sprite_num, buf, 7);
    }
  }

  void DVDisplay::clear_sprite(int sprite_num)
  {
    if (sprite_num < MAX_DISPLAYED_SPRITES) {
      uint8_t buf[3] = {1, 0xFF, 0xFF};
      i2c->write_bytes(I2C_ADDR, sprite_num, buf, 3);
    }
  }

  uint32_t DVDisplay::point_to_address(const Point& p) const {
    switch(mode) {
      default:
      case MODE_RGB555: return point_to_address16(p);
      case MODE_PALETTE: return point_to_address_palette(p);
      case MODE_RGB888: return point_to_address24(p);
    }
  }

  int DVDisplay::pixel_size() const {
    switch(mode) {
      default:
      case MODE_RGB555: return 2;
      case MODE_PALETTE: return 1;
      case MODE_RGB888: return 3;
    }
  }
}
