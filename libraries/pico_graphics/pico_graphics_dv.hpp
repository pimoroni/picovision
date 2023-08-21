#include "pico_graphics.hpp"

namespace pimoroni {
  class PicoGraphics_PenDV_RGB555 : public PicoGraphics {
    public:
      RGB555 color;
      IDirectDisplayDriver<uint16_t> &driver;

      PicoGraphics_PenDV_RGB555(uint16_t width, uint16_t height, IDirectDisplayDriver<uint16_t> &direct_display_driver);
      void set_pen(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;

      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB555);
      }
  };

  class PicoGraphics_PenDV_RGB888 : public PicoGraphics {
    public:
      RGB888 color;
      IDirectDisplayDriver<RGB888> &driver;

      PicoGraphics_PenDV_RGB888(uint16_t width, uint16_t height, IDirectDisplayDriver<RGB888> &direct_display_driver);
      void set_pen(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;

      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB888);
      }
  };

  class PicoGraphics_PenDV_P5 : public PicoGraphics {
    public:
      static const uint16_t palette_size = 32;
      uint8_t color;
      IPaletteDisplayDriver &driver;
      RGB palette[palette_size];
      bool used[palette_size];

      std::array<std::array<uint8_t, 16>, 512> candidate_cache;
      bool cache_built = false;
      std::array<uint8_t, 16> candidates;

      PicoGraphics_PenDV_P5(uint16_t width, uint16_t height, IPaletteDisplayDriver &dv_display);
      void set_pen(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      int reset_pen(uint8_t i) override;

      int get_palette_size() override {return palette_size;};
      RGB* get_palette() override {return palette;};

      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void get_dither_candidates(const RGB &col, const RGB *palette, size_t len, std::array<uint8_t, 16> &candidates);
      void set_pixel_dither(const Point &p, const RGB &c) override;

      static size_t buffer_size(uint w, uint h) {
          return w * h;
      }
  };
}