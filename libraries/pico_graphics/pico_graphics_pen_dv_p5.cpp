#include "pico_graphics_dv.hpp"

namespace pimoroni {

    inline constexpr uint32_t RGB_to_RGB888(const uint8_t r, const uint8_t g, const uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    PicoGraphics_PenDV_P5::PicoGraphics_PenDV_P5(uint16_t width, uint16_t height, DVDisplay &dv_display)
      : PicoGraphicsDV(width, height, dv_display)
      {
        this->pen_type = PEN_DV_P5;
        for(auto i = 0u; i < palette_size; i++) {
            driver.set_palette_colour(i, RGB_to_RGB888(i, i, i) << 3, 0);
            used[0][i] = false;
            int n = palette_size - i;
            driver.set_palette_colour(i, RGB_to_RGB888(n, n, n) << 3, 1);
            used[1][i] = false;
        }
        cache_built = false;
    }
    void PicoGraphics_PenDV_P5::set_pen(uint c) {
        color = c & 0x1f;
    }
    void PicoGraphics_PenDV_P5::set_depth(uint8_t new_depth) {
        depth = new_depth > 0 ? 1 : 0;
    }
    void PicoGraphics_PenDV_P5::set_pen(uint8_t r, uint8_t g, uint8_t b) {
        uint8_t current_palette = driver.get_local_palette_index();
        RGB888 *driver_palette = driver.get_palette(current_palette);
        RGB palette[palette_size];
        for(auto i = 0u; i < palette_size; i++) {
            palette[i] = RGB((uint)driver_palette[i]);
        }

        int pen = RGB(r, g, b).closest(palette, palette_size);
        if(pen != -1) color = pen;
    }
    int PicoGraphics_PenDV_P5::update_pen(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
        uint8_t current_palette = driver.get_local_palette_index();
        i &= 0x1f;
        used[current_palette][i] = true;
        cache_built = false;
        driver.set_palette_colour(i, RGB_to_RGB888(r, g, b));
        return i;
    }
    int PicoGraphics_PenDV_P5::create_pen(uint8_t r, uint8_t g, uint8_t b) {
        uint8_t current_palette = driver.get_local_palette_index();
        // Create a colour and place it in the palette if there's space
        for(auto i = 0u; i < palette_size; i++) {
            if(!used[current_palette][i]) {
                used[current_palette][i] = true;
                cache_built = false;
                driver.set_palette_colour(i, RGB_to_RGB888(r, g, b));
                return i;
            }
        }
        return -1;
    }
    int PicoGraphics_PenDV_P5::create_pen_hsv(float h, float s, float v) {
        RGB p = RGB::from_hsv(h, s, v);
        return create_pen(p.r, p.g, p.b);
    }
    int PicoGraphics_PenDV_P5::reset_pen(uint8_t i) {
        uint8_t current_palette = driver.get_local_palette_index();
        driver.set_palette_colour(i, 0);
        used[current_palette][i] = false;
        cache_built = false;
        return i;
    }
    void PicoGraphics_PenDV_P5::set_pixel(const Point &p) {
        driver.write_palette_pixel(p, (color << 2) | depth);
    }

    void PicoGraphics_PenDV_P5::set_pixel_span(const Point &p, uint l) {
        driver.write_palette_pixel_span(p, l, (color << 2) | depth);
    }

    void PicoGraphics_PenDV_P5::get_dither_candidates(const RGB &col, const RGB *palette, size_t len, std::array<uint8_t, 16> &candidates) {
        RGB error;
        for(size_t i = 0; i < candidates.size(); i++) {
            candidates[i] = (col + error).closest(palette, len);
            error += (col - palette[candidates[i]]);
        }

        // sort by a rough approximation of luminance, this ensures that neighbouring
        // pixels in the dither matrix are at extreme opposites of luminence
        // giving a more balanced output
        std::sort(candidates.begin(), candidates.end(), [palette](int a, int b) {
            return palette[a].luminance() > palette[b].luminance();
        });
    }

    void PicoGraphics_PenDV_P5::set_pixel_dither(const Point &p, const RGB &c) {
        if(!bounds.contains(p)) return;

        uint8_t current_palette = driver.get_local_palette_index();

        if(!cache_built || current_palette != cached_palette) {
            RGB888 *driver_palette = driver.get_palette(current_palette);
            RGB palette[palette_size];
            for(auto i = 0u; i < palette_size; i++) {
                palette[i] = RGB((uint)driver_palette[i]);
            }

            for(uint i = 0; i < 512; i++) {
                RGB cache_col((i & 0x1C0) >> 1, (i & 0x38) << 2, (i & 0x7) << 5);
                get_dither_candidates(cache_col, palette, palette_size, candidate_cache[i]);
            }
            cache_built = true;
            cached_palette = current_palette;
        }

        uint cache_key = ((c.r & 0xE0) << 1) | ((c.g & 0xE0) >> 2) | ((c.b & 0xE0) >> 5);

        // find the pattern coordinate offset
        uint pattern_index = (p.x & 0b11) | ((p.y & 0b11) << 2);

        // set the pixel
        color = candidate_cache[cache_key][dither16_pattern[pattern_index]];
        set_pixel(p);
    }
}
