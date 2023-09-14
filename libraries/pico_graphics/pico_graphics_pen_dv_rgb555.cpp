#include "pico_graphics_dv.hpp"

#ifndef MICROPY_BUILD_TYPE
#define mp_printf(_, ...) printf(__VA_ARGS__);
#else
extern "C" {
#include "py/runtime.h"
}
#endif

namespace pimoroni {
    PicoGraphics_PenDV_RGB555::PicoGraphics_PenDV_RGB555(uint16_t width, uint16_t height, DVDisplay &dv_display)
      : PicoGraphicsDV(width, height, dv_display)
    {
        this->pen_type = PEN_DV_RGB555;
    }
    void PicoGraphics_PenDV_RGB555::set_pen(uint c) {
        color = c;
    }
    void PicoGraphics_PenDV_RGB555::set_bg(uint c) {
        background = c;
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
        uint16_t src = background;
        if (blend_mode == BlendMode::TARGET) {
            driver.read_pixel_span(p, 1, &src);
        }

        uint8_t src_r = (src >> 7) & 0b11111000;
        uint8_t src_g = (src >> 2) & 0b11111000;
        uint8_t src_b = (src << 3) & 0b11111000;

        uint8_t dst_r = (color >> 7) & 0b11111000;
        uint8_t dst_g = (color >> 2) & 0b11111000;
        uint8_t dst_b = (color << 3) & 0b11111000;

        RGB555 blended = RGB(src_r, src_g, src_b).blend(RGB(dst_r, dst_g, dst_b), a).to_rgb555();

        driver.write_pixel(p, blended);
    };

    bool PicoGraphics_PenDV_RGB555::render_pico_vector_tile(const Rect &src_bounds, uint8_t* alpha_data, uint32_t stride, uint8_t alpha_type) {
        
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
        uint16_t buf0[128] alignas(4);
        uint16_t buf1[128] alignas(4);
        if (bounds.w > 128 || bounds.h > 128) {
            return false;
        }
        uint16_t* rbuf = buf0;
        uint16_t* wbuf = buf1;

        // Start reading the first row
        uint32_t address = driver.point_to_address({bounds.x, bounds.y});
        uint32_t row_len_in_words = (bounds.w + 1) >> 1;
        if (blend_mode == BlendMode::TARGET) {
            driver.raw_read_async(address, (uint32_t*)rbuf, row_len_in_words);
        }

        const uint32_t colour_expanded = (uint32_t(color & 0x7C00) << 10) | (uint32_t(color & 0x3E0) << 5) | (color & 0x1F);
        const uint32_t background_expanded = (uint32_t(background & 0x7C00) << 10) | (uint32_t(background & 0x3E0) << 5) | (background & 0x1F);
        uint32_t address_stride = driver.frame_row_stride();

        if (blend_mode == BlendMode::TARGET) {
            driver.raw_wait_for_finish_blocking();
        }

        for (int32_t y = 0; y < bounds.h; ++y) {
            std::swap(wbuf, rbuf);

            uint32_t prev_address = address;

            address += address_stride;

            // Process this row
            uint8_t* alpha_ptr = &alpha_data[stride * y];
            if (blend_mode == BlendMode::TARGET) {
                for (int32_t x = 0; x < bounds.w; ++x) {
                    uint8_t alpha = *alpha_ptr++;
                    if (alpha >= alpha_max) {
                        wbuf[x] = color;
                    } else if (alpha > 0) {
                        alpha = alpha_map[alpha];

                        uint16_t src = wbuf[x];

                        // Who said Cortex-M0 can't do SIMD?
                        const uint32_t src_expanded = (uint32_t(src & 0x7C00) << 10) | (uint32_t(src & 0x3E0) << 5) | (src & 0x1F);
                        const uint32_t blended = src_expanded * (16 - alpha) + colour_expanded * alpha;
                        wbuf[x] = ((blended >> 14) & 0x7C00) | ((blended >> 9) & 0x3E0) | ((blended >> 4) & 0x1F);
                    }

                    // Halfway through processing this row switch from writing previous row to reading the next
                    if (x+1 == (int32_t)row_len_in_words && y+1 < bounds.h) {
                        driver.raw_read_async(address, (uint32_t*)rbuf, row_len_in_words);
                    }
                }
            } else {
                for (int32_t x = 0; x < bounds.w; ++x) {
                    uint8_t alpha = *alpha_ptr++;
                    if (alpha >= alpha_max) {
                        wbuf[x] = color;
                    } else if (alpha > 0) {
                        alpha = alpha_map[alpha];

                        const uint32_t blended = background_expanded * (16 - alpha) + colour_expanded * alpha;
                        wbuf[x] = ((blended >> 14) & 0x7C00) | ((blended >> 9) & 0x3E0) | ((blended >> 4) & 0x1F);
                    } else {
                        wbuf[x] = background;
                    }
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