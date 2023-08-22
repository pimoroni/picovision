#include "pico_graphics_dv.hpp"

namespace pimoroni {
    PicoGraphics_PenDV_RGB888::PicoGraphics_PenDV_RGB888(uint16_t width, uint16_t height, IDirectDisplayDriver<RGB888> &direct_display_driver)
      : PicoGraphics(width, height, nullptr),
        driver(direct_display_driver) 
    {
        this->pen_type = PEN_DV_RGB888;
    }
    void PicoGraphics_PenDV_RGB888::set_pen(uint c) {
        color = c;
    }
    void PicoGraphics_PenDV_RGB888::set_pen(uint8_t r, uint8_t g, uint8_t b) {
        RGB src_color{r, g, b};
        color = src_color.to_rgb888(); 
    }
    int PicoGraphics_PenDV_RGB888::create_pen(uint8_t r, uint8_t g, uint8_t b) {
        return RGB(r, g, b).to_rgb888();
    }
    int PicoGraphics_PenDV_RGB888::create_pen_hsv(float h, float s, float v) {
        return RGB::from_hsv(h, s, v).to_rgb888();
    }
    void PicoGraphics_PenDV_RGB888::set_pixel(const Point &p) {
        driver.write_pixel(p, color);
    }
    void PicoGraphics_PenDV_RGB888::set_pixel_span(const Point &p, uint l) {
        driver.write_pixel_span(p, l, color);
    }
    void PicoGraphics_PenDV_RGB888::set_pixel_alpha(const Point &p, const uint8_t a) {
        uint32_t src = 0;

        driver.read_pixel_span(p, 2, &src);

        // TODO: "uint" should be RGB888 but there's curently a mismatch between "uint32_t" and "unsigned int"
        RGB888 blended = RGB((uint)src).blend(RGB((uint)color), a).to_rgb888();

        driver.write_pixel(p, blended);
    };
}