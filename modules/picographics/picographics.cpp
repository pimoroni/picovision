#include "drivers/dv_display/dv_display.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "common/pimoroni_common.hpp"

#include "micropython/modules/util.hpp"

using namespace pimoroni;

extern "C" {
#include "PNGdec.h"
#include "pngdec.h"
#include "picographics.h"
#include "pimoroni_i2c.h"
#include "py/stream.h"
#include "py/reader.h"
#include "extmod/vfs.h"

const std::string_view mp_obj_to_string_r(const mp_obj_t &obj) {
    if(mp_obj_is_str_or_bytes(obj)) {
        GET_STR_DATA_LEN(obj, str, str_len);
        return std::string_view((const char*)str, str_len);
    }
    mp_raise_TypeError("can't convert object to str implicitly");
}

static I2C dv_i2c(DVDisplay::I2C_SDA, DVDisplay::I2C_SCL);
static DVDisplay dv_display(&dv_i2c);

typedef struct _ModPicoGraphics_obj_t {
    mp_obj_base_t base;
    PicoGraphics *graphics;
    DVDisplay *display;
} ModPicoGraphics_obj_t;

typedef struct _PNG_obj_t {
    mp_obj_base_t base;
    PNG *png;
    void *dither_buffer;
    mp_obj_t file;
    mp_buffer_info_t buf;
    PNG_DRAW_CALLBACK *decode_callback;
    void *decode_target;
    bool decode_into_buffer;
    int width;
    int height;
} _PNG_obj_t;

size_t get_required_buffer_size(PicoGraphicsPenType pen_type, uint width, uint height) {
    switch(pen_type) {
        case PEN_DV_RGB888:
            return PicoGraphics_PenDV_RGB888::buffer_size(width, height);
        case PEN_DV_RGB555:
            return PicoGraphics_PenDV_RGB555::buffer_size(width, height);
        case PEN_DV_P5:
            return PicoGraphics_PenDV_P5::buffer_size(width, height);
        default:
            return 0;
    }
}

void PNGDrawSprite(PNGDRAW *pDraw) {
#ifdef MICROPY_EVENT_POLL_HOOK
MICROPY_EVENT_POLL_HOOK
#endif
    // "pixel" is slow and clipped,
    // guaranteeing we wont draw png data out of the framebuffer..
    // Can we clip beforehand and make this faster?
    int y = pDraw->y;

    uint16_t *sprite_data = (uint16_t *)pDraw->pUser;
    sprite_data += y * pDraw->iWidth;

    // TODO we need to handle PicoGraphics palette pen types, plus grayscale and gray alpha PNG modes
    // For DV P5 we could copy over the raw palette values (p & 0b00011111) and assume the user has prepped their palette accordingly
    // also dither, maybe?

    //mp_printf(&mp_plat_print, "Read sprite line at %d, %dbpp, type: %d, width: %d pitch: %d alpha: %d\n", y, pDraw->iBpp, pDraw->iPixelType, pDraw->iWidth, pDraw->iPitch, pDraw->iHasAlpha);
    uint8_t *pixel = (uint8_t *)pDraw->pPixels;
    if(pDraw->iPixelType == PNG_PIXEL_TRUECOLOR ) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint16_t r = *pixel++;
            uint16_t g = *pixel++;
            uint16_t b = *pixel++;
            *sprite_data++ = 0x8000 | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | (b >> 3);
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint16_t r = *pixel++;
            uint16_t g = *pixel++;
            uint16_t b = *pixel++;
            uint8_t a = *pixel++;
            *sprite_data++ = (a ? 0x8000 : 0) | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | (b >> 3);
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_INDEXED) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint8_t i = 0;
            if(pDraw->iBpp == 8) {
                i = *pixel++;
            } else {
                i = pixel[x / 2];
                i >>= (x & 0b1) ? 0 : 4;
                i &= 0xf;
            }
            uint16_t r = pDraw->pPalette[(i * 3) + 0];
            uint16_t g = pDraw->pPalette[(i * 3) + 1];
            uint16_t b = pDraw->pPalette[(i * 3) + 2];
            uint8_t a = pDraw->iHasAlpha ? pDraw->pPalette[768 + i] : 1;
            *sprite_data++ = (a ? 0x8000 : 0) | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | (b >> 3);
        }
    }
}

mp_obj_t ModPicoGraphics_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    ModPicoGraphics_obj_t *self = nullptr;

    enum { ARG_display, ARG_pen_type, ARG_width, ARG_height, ARG_frame_width, ARG_frame_height };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_display, MP_ARG_INT | MP_ARG_REQUIRED },
        { MP_QSTR_pen_type, MP_ARG_INT, { .u_int = PEN_DV_RGB888 } },
        { MP_QSTR_width, MP_ARG_INT, { .u_int = 320 } },
        { MP_QSTR_height, MP_ARG_INT, { .u_int = 240 } },
        { MP_QSTR_frame_width, MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_frame_height, MP_ARG_INT, { .u_int = -1 } }
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    self = m_new_obj_with_finaliser(ModPicoGraphics_obj_t);
    self->base.type = &ModPicoGraphics_type;

    int width = args[ARG_width].u_int;
    int height = args[ARG_height].u_int;
    int frame_width = args[ARG_frame_width].u_int == -1 ? width : args[ARG_frame_width].u_int;
    int frame_height = args[ARG_frame_height].u_int == -1 ? height : args[ARG_frame_height].u_int;
    int pen_type = args[ARG_pen_type].u_int;

    // Create an instance of the graphics library and DV display driver
    switch((PicoGraphicsPenType)pen_type) {
        case PEN_DV_RGB888:
            self->graphics = m_new_class(PicoGraphics_PenDV_RGB888, frame_width, frame_height, *(IDirectDisplayDriver<RGB888> *)&dv_display);
            dv_display.init(width, height, DVDisplay::MODE_RGB888, frame_width, frame_height);
            dv_display.set_mode(DVDisplay::MODE_RGB888);
            break;
        case PEN_DV_RGB555:
            self->graphics = m_new_class(PicoGraphics_PenDV_RGB555, frame_width, frame_height, *(IDirectDisplayDriver<uint16_t> *)&dv_display);
            dv_display.init(width, height, DVDisplay::MODE_RGB555, frame_width, frame_height);
            dv_display.set_mode(DVDisplay::MODE_RGB555);
            break;
        case PEN_DV_P5:
            self->graphics = m_new_class(PicoGraphics_PenDV_P5, frame_width, frame_height, *(IPaletteDisplayDriver *)&dv_display);
            dv_display.init(width, height, DVDisplay::MODE_PALETTE, frame_width, frame_height);
            dv_display.set_mode(DVDisplay::MODE_PALETTE);
            break;
        default:
            break;
    }

    self->display = &dv_display;

    // Clear each buffer
    for(auto x = 0u; x < 2u; x++){
        self->graphics->set_pen(0);
        self->graphics->clear();
        dv_display.flip();
    }

    return MP_OBJ_FROM_PTR(self);
}

mp_obj_t ModPicoGraphics__del__(mp_obj_t self_in) {
    dv_display.reset();
    return mp_const_none;
}

// DV Display specific functions
mp_obj_t ModPicoGraphics_set_display_offset(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_scroll_index };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_scroll_index, MP_ARG_INT, { .u_int = 1 } }
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    dv_display.set_display_offset(
        Point(
            args[ARG_x].u_int,
            args[ARG_y].u_int
        ),
        args[ARG_scroll_index].u_int
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_scroll_index_for_lines(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_scroll_index, ARG_min_y, ARG_max_y };

    dv_display.set_scroll_idx_for_lines(
        mp_obj_get_int(args[ARG_scroll_index]),
        mp_obj_get_int(args[ARG_min_y]),
        mp_obj_get_int(args[ARG_max_y]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_load_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_filename, ARG_index, ARG_scale, ARG_dither };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_filename, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_index, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_dither, MP_ARG_OBJ, {.u_obj = mp_const_true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    // Try loading the file/stream and checking if it's a PNG graphic
    // if it's *not* a PNG then assume it's a raw PicoVision sprite.
    const char PNG_HEADER[] = {137, 80, 78, 71, 13, 10, 26, 10};

    // Is the supplied filename a string or bytearray
    if(mp_obj_is_str_or_bytes(args[ARG_filename].u_obj)){
        GET_STR_DATA_LEN(args[ARG_filename].u_obj, str, str_len);

        // It's a file, try opening it
        int32_t fsize;
        mp_obj_t *fhandle = (mp_obj_t *)pngdec_open_callback((const char*)str, &fsize);
        void *buf = m_malloc(fsize);
        int error;

        mp_stream_read_exactly(fhandle, buf, sizeof(PNG_HEADER), &error);

        if (strncmp((const char *)buf, PNG_HEADER, sizeof(PNG_HEADER)) != 0) {
            mp_printf(&mp_plat_print, "Not a PNG, loading as a PVS sprite.\n");
            mp_stream_read_exactly(fhandle, (uint8_t *)buf + sizeof(PNG_HEADER), fsize - sizeof(PNG_HEADER), &error);
            self->display->load_pvs_sprite(args[ARG_index].u_int, (uint32_t *)buf, fsize);

            return mp_const_none;
        }

    } else {
        // It might be a buffer...
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_filename].u_obj, &bufinfo, MP_BUFFER_READ);

        if (strncmp((const char *)bufinfo.buf, PNG_HEADER, sizeof(PNG_HEADER)) != 0) {
            mp_printf(&mp_plat_print, "Not a PNG, loading as a PVS sprite.\n");
            self->display->load_pvs_sprite(args[ARG_index].u_int , (uint32_t*)bufinfo.buf, bufinfo.len);
            m_del(uint8_t, bufinfo.buf, bufinfo.len);

            return mp_const_none;
        }

        m_del(uint8_t, bufinfo.buf, bufinfo.len);
    }

    // Construct a pngdec object with our callback and a null buffer
    // ask it to decode into a new, correctly-sized buffer
    _PNG_obj_t *pngdec = m_new_obj_with_finaliser(_PNG_obj_t);
    pngdec->base.type = &PNG_type;
    pngdec->png = m_new_class(PNG);
    pngdec->decode_callback = PNGDrawSprite;
    pngdec->decode_target = nullptr;
    pngdec->decode_into_buffer = true;
    pngdec->width = 0;
    pngdec->height = 0;

    // Open the file, sets the width/height values
    _PNG_openFILE(pngdec, args[ARG_filename].u_obj);

    // Create a buffer big enough to hold the data decoded from PNG by our draw callback
    pngdec->decode_target = m_malloc(pngdec->width * pngdec->height * sizeof(uint16_t));

    mp_obj_t decode_args[] = {
        pngdec,
        mp_obj_new_int(0),
        mp_obj_new_int(0),
        mp_obj_new_int(args[ARG_scale].u_int),
        mp_const_false
    };
    _PNG_decode(MP_ARRAY_SIZE(args), &decode_args[0], (mp_map_t *)&mp_const_empty_map);

    self->display->define_sprite(args[ARG_index].u_int, pngdec->width, pngdec->height, (uint16_t *)pngdec->decode_target);

    m_free(pngdec->decode_target);
    m_free(pngdec->png);
    m_free(pngdec);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_display_sprite(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_slot, ARG_sprite_index, ARG_x, ARG_y };

    dv_display.set_sprite(mp_obj_get_int(args[ARG_slot]), 
                          mp_obj_get_int(args[ARG_sprite_index]),
                          {mp_obj_get_int(args[ARG_x]), mp_obj_get_int(args[ARG_y])});

    return mp_const_true;
}

mp_obj_t ModPicoGraphics_clear_sprite(mp_obj_t self_in, mp_obj_t slot) {
    dv_display.clear_sprite(mp_obj_get_int(slot));
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_font(mp_obj_t self_in, mp_obj_t font) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    self->graphics->set_font(mp_obj_to_string_r(font));
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_get_bounds(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    mp_obj_t tuple[2] = {
        mp_obj_new_int(self->graphics->bounds.w),
        mp_obj_new_int(self->graphics->bounds.h)
    };
    return mp_obj_new_tuple(2, tuple);
}

mp_obj_t ModPicoGraphics_update(mp_obj_t self_in) {
    (void)self_in;
    dv_display.flip();
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_module_RGB332_to_RGB(mp_obj_t rgb332) {
    RGB c((RGB332)mp_obj_get_int(rgb332));
    mp_obj_t t[] = {
        mp_obj_new_int(c.r),
        mp_obj_new_int(c.g),
        mp_obj_new_int(c.b),
    };
    return mp_obj_new_tuple(3, t);
}

mp_obj_t ModPicoGraphics_module_RGB565_to_RGB(mp_obj_t rgb565) {
    RGB c((RGB565)mp_obj_get_int(rgb565));
    mp_obj_t t[] = {
        mp_obj_new_int(c.r),
        mp_obj_new_int(c.g),
        mp_obj_new_int(c.b),
    };
    return mp_obj_new_tuple(3, t);
}

mp_obj_t ModPicoGraphics_module_RGB_to_RGB332(mp_obj_t r, mp_obj_t g, mp_obj_t b) {
    return mp_obj_new_int(RGB(
        mp_obj_get_int(r),
        mp_obj_get_int(g),
        mp_obj_get_int(b)
    ).to_rgb332());
}

mp_obj_t ModPicoGraphics_module_RGB_to_RGB565(mp_obj_t r, mp_obj_t g, mp_obj_t b) {
    return mp_obj_new_int(RGB(
        mp_obj_get_int(r),
        mp_obj_get_int(g),
        mp_obj_get_int(b)
    ).to_rgb565());
}

mp_obj_t ModPicoGraphics_set_pen(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_pen(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_reset_pen(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->reset_pen(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_update_pen(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_i, ARG_r, ARG_g, ARG_b };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->update_pen(
        mp_obj_get_int(args[ARG_i]) & 0xff,
        mp_obj_get_int(args[ARG_r]) & 0xff,
        mp_obj_get_int(args[ARG_g]) & 0xff,
        mp_obj_get_int(args[ARG_b]) & 0xff
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_create_pen(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_r, ARG_g, ARG_b };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    int result = self->graphics->create_pen(
        mp_obj_get_int(args[ARG_r]) & 0xff,
        mp_obj_get_int(args[ARG_g]) & 0xff,
        mp_obj_get_int(args[ARG_b]) & 0xff
    );

    if (result == -1) mp_raise_ValueError("create_pen failed. No matching colour or space in palette!");

    return mp_obj_new_int(result);
}

mp_obj_t ModPicoGraphics_create_pen_hsv(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_h, ARG_s, ARG_v };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);
    int result = self->graphics->create_pen_hsv(
        mp_obj_get_float(args[ARG_h]),
        mp_obj_get_float(args[ARG_s]),
        mp_obj_get_float(args[ARG_v])
    );

    if (result == -1) mp_raise_ValueError("create_pen failed. No matching colour or space in palette!");

    return mp_obj_new_int(result);
}

mp_obj_t ModPicoGraphics_set_thickness(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_thickness(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_palette(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    size_t num_tuples = n_args - 1;
    const mp_obj_t *tuples = pos_args + 1;

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(pos_args[0], ModPicoGraphics_obj_t);

    // Check if there is only one argument, which might be a list
    if(n_args == 2) {
        if(mp_obj_is_type(pos_args[1], &mp_type_list)) {
            mp_obj_list_t *points = MP_OBJ_TO_PTR2(pos_args[1], mp_obj_list_t);

            if(points->len <= 0) mp_raise_ValueError("set_palette(): cannot provide an empty list");

            num_tuples = points->len;
            tuples = points->items;
        }
        else {
            mp_raise_TypeError("set_palette(): can't convert object to list");
        }
    }

    for(size_t i = 0; i < num_tuples; i++) {
        mp_obj_t obj = tuples[i];
        if(!mp_obj_is_type(obj, &mp_type_tuple)) mp_raise_ValueError("set_palette(): can't convert object to tuple");

        mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(obj, mp_obj_tuple_t);

        if(tuple->len != 3) mp_raise_ValueError("set_palette(): tuple must contain R, G, B values");

        self->graphics->update_pen(
            i,
            mp_obj_get_int(tuple->items[0]),
            mp_obj_get_int(tuple->items[1]),
            mp_obj_get_int(tuple->items[2])
        );
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_clip(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_w, ARG_h };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->set_clip({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y]),
        mp_obj_get_int(args[ARG_w]),
        mp_obj_get_int(args[ARG_h])
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_remove_clip(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->remove_clip();

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_clear(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->clear();

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_pixel(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->pixel({
        mp_obj_get_int(x),
        mp_obj_get_int(y)
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_pixel_span(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_l };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->pixel_span({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y])
    },  mp_obj_get_int(args[ARG_l]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_rectangle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_w, ARG_h };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->rectangle({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y]),
        mp_obj_get_int(args[ARG_w]),
        mp_obj_get_int(args[ARG_h])
    });

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_circle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_r };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->circle({
        mp_obj_get_int(args[ARG_x]),
        mp_obj_get_int(args[ARG_y])
    },  mp_obj_get_int(args[ARG_r]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_character(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_char, ARG_x, ARG_y, ARG_scale };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_char, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 2} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    int c = mp_obj_get_int(args[ARG_char].u_obj);
    int x = args[ARG_x].u_int;
    int y = args[ARG_y].u_int;
    int scale = args[ARG_scale].u_int;

    self->graphics->character((char)c, Point(x, y), scale);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_text, ARG_x, ARG_y, ARG_wrap, ARG_scale, ARG_angle, ARG_spacing };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_wordwrap, MP_ARG_INT, {.u_int = __INT32_MAX__} },  // Sort-of a fudge, but wide-enough to avoid wrapping on any display size
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_angle, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    mp_obj_t text_obj = args[ARG_text].u_obj;

    if(!mp_obj_is_str_or_bytes(text_obj)) mp_raise_TypeError("text: string required");

    GET_STR_DATA_LEN(text_obj, str, str_len);

    const std::string_view t((const char*)str, str_len);

    int x = args[ARG_x].u_int;
    int y = args[ARG_y].u_int;
    int wrap = args[ARG_wrap].u_int;
    float scale = args[ARG_scale].u_obj == mp_const_none ? 2.0f : mp_obj_get_float(args[ARG_scale].u_obj);
    int angle = args[ARG_angle].u_int;
    int letter_spacing = args[ARG_spacing].u_int;

    self->graphics->text(t, Point(x, y), wrap, scale, angle, letter_spacing);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_measure_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_text, ARG_scale, ARG_spacing };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    mp_obj_t text_obj = args[ARG_text].u_obj;

    if(!mp_obj_is_str_or_bytes(text_obj)) mp_raise_TypeError("text: string required");

    GET_STR_DATA_LEN(text_obj, str, str_len);

    const std::string_view t((const char*)str, str_len);

    float scale = args[ARG_scale].u_obj == mp_const_none ? 2.0f : mp_obj_get_float(args[ARG_scale].u_obj);
    int letter_spacing = args[ARG_spacing].u_int;

    int width = self->graphics->measure_text(t, scale, letter_spacing);

    return mp_obj_new_int(width);
}

mp_obj_t ModPicoGraphics_polygon(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    size_t num_tuples = n_args - 1;
    const mp_obj_t *tuples = pos_args + 1;

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(pos_args[0], ModPicoGraphics_obj_t);

    // Check if there is only one argument, which might be a list
    if(n_args == 2) {
        if(mp_obj_is_type(pos_args[1], &mp_type_list)) {
            mp_obj_list_t *points = MP_OBJ_TO_PTR2(pos_args[1], mp_obj_list_t);

            if(points->len <= 0) mp_raise_ValueError("poly(): cannot provide an empty list");

            num_tuples = points->len;
            tuples = points->items;
        }
        else {
            mp_raise_TypeError("poly(): can't convert object to list");
        }
    }

    if(num_tuples > 0) {
        std::vector<Point> points;
        for(size_t i = 0; i < num_tuples; i++) {
            mp_obj_t obj = tuples[i];
            if(!mp_obj_is_type(obj, &mp_type_tuple)) mp_raise_ValueError("poly(): can't convert object to tuple");

            mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(obj, mp_obj_tuple_t);

            if(tuple->len != 2) mp_raise_ValueError("poly(): tuple must only contain two numbers");

            points.push_back({
                mp_obj_get_int(tuple->items[0]),
                mp_obj_get_int(tuple->items[1])
            });
        }
        self->graphics->polygon(points);
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_triangle(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x1, ARG_y1, ARG_x2, ARG_y2, ARG_x3, ARG_y3 };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    self->graphics->triangle(
        {mp_obj_get_int(args[ARG_x1]),
         mp_obj_get_int(args[ARG_y1])},
        {mp_obj_get_int(args[ARG_x2]),
         mp_obj_get_int(args[ARG_y2])},
        {mp_obj_get_int(args[ARG_x3]),
         mp_obj_get_int(args[ARG_y3])}
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_line(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_x1, ARG_y1, ARG_x2, ARG_y2, ARG_thickness };

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self], ModPicoGraphics_obj_t);

    if(n_args == 5) {
        self->graphics->line(
            {mp_obj_get_int(args[ARG_x1]),
            mp_obj_get_int(args[ARG_y1])},
            {mp_obj_get_int(args[ARG_x2]),
            mp_obj_get_int(args[ARG_y2])}
        );
    }
    else if(n_args == 6) {
        self->graphics->thick_line(
            {mp_obj_get_int(args[ARG_x1]),
            mp_obj_get_int(args[ARG_y1])},
            {mp_obj_get_int(args[ARG_x2]),
            mp_obj_get_int(args[ARG_y2])},
            mp_obj_get_int(args[ARG_thickness])
        );
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_get_i2c(mp_obj_t self_in) {
    _PimoroniI2C_obj_t* i2c_obj = m_new_obj(_PimoroniI2C_obj_t);
    i2c_obj->base.type = &PimoroniI2C_type;

    i2c_obj->i2c = &dv_i2c;

    return i2c_obj;
}
}
