#include "pico_graphics.hpp"
#include "dv_display.hpp"

namespace pimoroni {
  enum BlendMode {
    TARGET = 0,
    FIXED = 1,
  };

  class PicoGraphicsDV : public PicoGraphics {
    public:
      DVDisplay &driver;
      BlendMode blend_mode = BlendMode::TARGET;

      void set_blend_mode(BlendMode mode) {
        blend_mode = mode;
      };

      virtual void set_bg(uint c) {};

      PicoGraphicsDV(uint16_t width, uint16_t height, DVDisplay &dv_display)
      : PicoGraphics(width, height, nullptr),
        driver(dv_display)
      {
          this->pen_type = PEN_DV_RGB555;
      }
  };

  class PicoGraphics_PenDV_RGB555 : public PicoGraphicsDV {
    public:
      RGB555 color;
      RGB555 background;

      PicoGraphics_PenDV_RGB555(uint16_t width, uint16_t height, DVDisplay &dv_display);
      void set_pen(uint c) override;
      void set_bg(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void set_pixel_alpha(const Point &p, const uint8_t a) override;

      bool supports_alpha_blend() override {return true;}

      bool render_pico_vector_tile(const Rect &bounds, uint8_t* alpha_data, uint32_t stride, uint8_t alpha_type) override;

      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB555);
      }
  };

  class PicoGraphics_PenDV_RGB888 : public PicoGraphicsDV {
    public:
      RGB888 color;
      RGB888 background;

      PicoGraphics_PenDV_RGB888(uint16_t width, uint16_t height, DVDisplay &dv_display);
      void set_pen(uint c) override;
      void set_bg(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void set_pixel_alpha(const Point &p, const uint8_t a) override;

      bool supports_alpha_blend() override {return true;}

      static size_t buffer_size(uint w, uint h) {
        return w * h * sizeof(RGB888);
      }
  };

  class PicoGraphics_PenDV_P5 : public PicoGraphicsDV {
    public:
      static const uint16_t palette_size = 32;
      uint8_t color;
      bool used[2][palette_size];

      std::array<std::array<uint8_t, 16>, 512> candidate_cache;
      std::array<uint8_t, 16> candidates;
      bool cache_built = false;
      uint8_t cached_palette = 0;

      PicoGraphics_PenDV_P5(uint16_t width, uint16_t height, DVDisplay &dv_display);
      void set_pen(uint c) override;
      void set_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen(uint8_t r, uint8_t g, uint8_t b) override;
      int create_pen_hsv(float h, float s, float v) override;
      int reset_pen(uint8_t i) override;

      int get_palette_size() override {return 0;};
      RGB* get_palette() override {return nullptr;};

      void set_pixel(const Point &p) override;
      void set_pixel_span(const Point &p, uint l) override;
      void get_dither_candidates(const RGB &col, const RGB *palette, size_t len, std::array<uint8_t, 16> &candidates);
      void set_pixel_dither(const Point &p, const RGB &c) override;

      static size_t buffer_size(uint w, uint h) {
          return w * h;
      }
  };
}