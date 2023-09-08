#include "pico_graphics_dv.hpp"

namespace pimoroni {
    PicoGraphics_PenDV_RGB555::PicoGraphics_PenDV_RGB555(uint16_t width, uint16_t height, DVDisplay &dv_display)
      : PicoGraphics(width, height, nullptr),
        driver(dv_display)
    {
        this->pen_type = PEN_DV_RGB555;
    }
    void PicoGraphics_PenDV_RGB555::set_pen(uint c) {
        color = c;
    }
    void PicoGraphics_PenDV_RGB555::set_pen(uint8_t r, uint8_t g, uint8_t b) {
        RGB src_color{r, g, b};
        color = src_color.to_rgb555(); 
    }
    int PicoGraphics_PenDV_RGB555::create_pen(uint8_t r, uint8_t g, uint8_t b) {
        return RGB(r, g, b).to_rgb555();
    }
    int PicoGraphics_PenDV_RGB555::create_pen_hsv(float h, float s, float v) {
        return RGB::from_hsv(h, s, v).to_rgb555();
    }
    void PicoGraphics_PenDV_RGB555::set_pixel(const Point &p) {
        driver.write_pixel(p, color);
    }
    void PicoGraphics_PenDV_RGB555::set_pixel_span(const Point &p, uint l) {
        driver.write_pixel_span(p, l, color);
    }
    void PicoGraphics_PenDV_RGB555::set_pixel_alpha(const Point &p, const uint8_t a) {
        uint16_t src = 0;
        driver.read_pixel_span(p, 1, &src);

        uint8_t src_r = (src >> 7) & 0b11111000;
        uint8_t src_g = (src >> 2) & 0b11111000;
        uint8_t src_b = (src << 3) & 0b11111000;

        uint8_t dst_r = (color >> 7) & 0b11111000;
        uint8_t dst_g = (color >> 2) & 0b11111000;
        uint8_t dst_b = (color << 3) & 0b11111000;

        RGB555 blended = RGB(src_r, src_g, src_b).blend(RGB(dst_r, dst_g, dst_b), a).to_rgb555();

        driver.write_pixel(p, blended);
    };
}