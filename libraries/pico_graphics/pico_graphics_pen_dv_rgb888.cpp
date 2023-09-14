#include "pico_graphics_dv.hpp"

namespace pimoroni {
    PicoGraphics_PenDV_RGB888::PicoGraphics_PenDV_RGB888(uint16_t width, uint16_t height, DVDisplay &dv_display)
      : PicoGraphicsDV(width, height, dv_display)
    {
        this->pen_type = PEN_DV_RGB888;
    }
    void PicoGraphics_PenDV_RGB888::set_pen(uint c) {
        color = c;
    }
    void PicoGraphics_PenDV_RGB888::set_bg(uint c) {
        background = c;
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
        uint32_t src = background;
        if (blend_mode == BlendMode::TARGET) {
            driver.read_24bpp_pixel_span(p, 1, (uint8_t*)&src);
        }

        // TODO: "uint" should be RGB888 but there's curently a mismatch between "uint32_t" and "unsigned int"
        RGB888 blended = RGB((uint)src).blend(RGB((uint)color), a).to_rgb888();

        driver.write_pixel(p, blended);
    };

    bool PicoGraphics_PenDV_RGB888::render_pico_vector_tile(const Rect &src_bounds, uint8_t* alpha_data, uint32_t stride, uint8_t alpha_type) {
        
        // Alpha configuration
        const uint8_t alpha_map4[4] = { 0, 8, 12, 15 };
        const uint8_t alpha_map16[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        const uint8_t* alpha_map = (alpha_type == 2) ? alpha_map16 : alpha_map4;
        const uint8_t alpha_max = (alpha_type == 2) ? 16 : 4;

        const Rect bounds = src_bounds.intersection(clip);
        if (bounds.w <= 0 || bounds.h <= 0) return true;
        alpha_data += bounds.x - src_bounds.x + stride * (bounds.y - src_bounds.y);
        
        // Double buffering - in the main loop one buffer is being written to the PSRAM or read into 
        // in the background while the other is processed
        uint8_t buf0[128*3] alignas(4);
        uint8_t buf1[128*3] alignas(4);
        if (bounds.w > 128 || bounds.h > 128) {
            return false;
        }
        uint8_t* rbuf = buf0;
        uint8_t* wbuf = buf1;

        // Start reading the first row
        uint32_t address = driver.point_to_address({bounds.x, bounds.y});
        uint32_t row_len_in_words = (bounds.w * 3 + 3) >> 2;
        driver.raw_read_async(address, (uint32_t*)rbuf, row_len_in_words);

        const uint8_t colour_expanded[3] = { (uint8_t)color, (uint8_t)(color >> 8), (uint8_t)(color >> 16) };
        uint32_t address_stride = driver.frame_row_stride();

        driver.raw_wait_for_finish_blocking();

        for (int32_t y = 0; y < bounds.h; ++y) {
            std::swap(wbuf, rbuf);

            uint32_t prev_address = address;

            address += address_stride;

            // Process this row
            uint8_t* alpha_ptr = &alpha_data[stride * y];
            for (int32_t x = 0; x < bounds.w; ++x) {
                uint8_t alpha = *alpha_ptr++;
                if (alpha >= alpha_max) {
                    wbuf[x*3] = colour_expanded[0];
                    wbuf[x*3+1] = colour_expanded[1];
                    wbuf[x*3+2] = colour_expanded[2];
                } else if (alpha > 0) {

                    alpha = alpha_map[alpha];
                    for (int32_t i = 0; i < 3; ++i) {
                        uint8_t src = wbuf[x*3+i];
                        wbuf[x*3+i] = (src * (16 - alpha) + colour_expanded[i] * alpha) >> 4;
                    }
                }

                // Halfway through processing this row switch from writing previous row to reading the next
                if (x == bounds.w >> 1 && y+1 < bounds.h) {
                    driver.raw_read_async(address, (uint32_t*)rbuf, row_len_in_words);
                }
            }

            // Write the row out while we loop on to the next
            driver.raw_write_async(prev_address, (uint32_t*)wbuf, row_len_in_words);
        }

        // Wait for the last write to finish as we are writing from stack
        driver.raw_wait_for_finish_blocking();

        return true;
    }    
}