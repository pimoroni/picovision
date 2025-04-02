#include "drivers/dv_display/dv_display.hpp"
#include "libraries/pico_graphics/pico_graphics_dv.hpp"
#include "common/pimoroni_common.hpp"

#include "micropython/modules/util.hpp"

using namespace pimoroni;


namespace {
    class DV_preinit {
        public:
        DV_preinit() {
            DVDisplay::preinit();
        }
    };
    DV_preinit dv_preinit __attribute__ ((init_priority (101))) ;
}

extern "C" {
#include "PNGdec.h"
#include "picographics.h"
#include "pimoroni_i2c.h"
#include "py/stream.h"
#include "py/reader.h"
#include "py/objarray.h"
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
    PicoGraphicsDV *graphics;
    DVDisplay *display;
} ModPicoGraphics_obj_t;

typedef struct _PNG_decode_target {
    void *target;
    Rect source = {0, 0, 0, 0};
} _PNG_decode_target;

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

//  Use file IO functions from PNGDEC module
extern void *pngdec_open_callback(const char *filename, int32_t *size);
extern void pngdec_close_callback(void *handle);
extern int32_t pngdec_read_callback(PNGFILE *png, uint8_t *p, int32_t c);
extern int32_t pngdec_seek_callback(PNGFILE *png, int32_t p);

void PNGDrawSprite_Indexed(PNGDRAW *pDraw) {
#ifdef MICROPY_EVENT_POLL_HOOK
MICROPY_EVENT_POLL_HOOK
#endif
    _PNG_decode_target *target = (_PNG_decode_target*)pDraw->pUser;

    if(pDraw->y < target->source.y || pDraw->y >= target->source.y + target->source.h) return;

    uint8_t *sprite_data = (uint8_t *)target->target;
    sprite_data += (pDraw->y - target->source.y) * target->source.w;

    //mp_printf(&mp_plat_print, "Decoding scanline at %d, %dbpp, type: %d, width: %d pitch: %d alpha: %d\n", pDraw->y, pDraw->iBpp, pDraw->iPixelType, pDraw->iWidth, pDraw->iPitch, pDraw->iHasAlpha);

    uint8_t *pixel = (uint8_t *)pDraw->pPixels;
    if (pDraw->iPixelType == PNG_PIXEL_INDEXED) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint8_t i = 0;
            if(pDraw->iBpp == 8) {
                i = *pixel++;
            } else if (pDraw->iBpp == 4) {
                i = *pixel;
                i >>= (x & 0b1) ? 0 : 4;
                i &= 0xf;
                if (x & 1) pixel++;
            } else {
                i = *pixel;
                i >>= 6 - ((x & 0b11) << 1);
                i &= 0x3;
                if ((x & 0b11) == 0b11) pixel++;
            }
            if(x < target->source.x || x >= target->source.x + target->source.w) continue;

            uint8_t a = (pDraw->iHasAlpha && pDraw->pPalette[768 + i] > 0) ? 0x01 : 0x00;

            *sprite_data++ = ((i & 0x1f) << 2) | a;
        }
    }
}

void PNGDrawSprite(PNGDRAW *pDraw) {
#ifdef MICROPY_EVENT_POLL_HOOK
MICROPY_EVENT_POLL_HOOK
#endif
    _PNG_decode_target *target = (_PNG_decode_target*)pDraw->pUser;

    if(pDraw->y < target->source.y || pDraw->y >= target->source.y + target->source.h) return;

    uint16_t *sprite_data = (uint16_t *)target->target;
    sprite_data += (pDraw->y - target->source.y) * target->source.w;

    // TODO we need to handle PicoGraphics palette pen types, plus grayscale and gray alpha PNG modes
    // For DV P5 we could copy over the raw palette values (p & 0b00011111) and assume the user has prepped their palette accordingly
    // also dither, maybe?

    //mp_printf(&mp_plat_print, "Read sprite line at %d, %dbpp, type: %d, width: %d pitch: %d alpha: %d\n", pDraw->y, pDraw->iBpp, pDraw->iPixelType, pDraw->iWidth, pDraw->iPitch, pDraw->iHasAlpha);
    uint8_t *pixel = (uint8_t *)pDraw->pPixels;
    if(pDraw->iPixelType == PNG_PIXEL_TRUECOLOR ) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint16_t r = *pixel++;
            uint16_t g = *pixel++;
            uint16_t b = *pixel++;
            if(x < target->source.x || x >= target->source.x + target->source.w) continue;
            *sprite_data++ = 0x8000 | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | (b >> 3);
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint16_t r = *pixel++;
            uint16_t g = *pixel++;
            uint16_t b = *pixel++;
            uint8_t a = *pixel++;
            if(x < target->source.x || x >= target->source.x + target->source.w) continue;
            *sprite_data++ = (a ? 0x8000 : 0) | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | (b >> 3);
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_INDEXED) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint8_t i = 0;
            if(pDraw->iBpp == 8) {
                i = *pixel++;
            } else if (pDraw->iBpp == 4) {
                i = *pixel;
                i >>= (x & 0b1) ? 0 : 4;
                i &= 0xf;
                if (x & 1) pixel++;
            } else {
                i = *pixel;
                i >>= 6 - ((x & 0b11) << 1);
                i &= 0x3;
                if ((x & 0b11) == 0b11) pixel++;
            }
            if(x < target->source.x || x >= target->source.x + target->source.w) continue;
            // grab the colour from the palette
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

    enum { ARG_pen_type, ARG_width, ARG_height, ARG_frame_width, ARG_frame_height, ARG_maximum_compatibility };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pen_type, MP_ARG_INT, { .u_int = PEN_DV_RGB888 } },
        { MP_QSTR_width, MP_ARG_INT, { .u_int = 320 } },
        { MP_QSTR_height, MP_ARG_INT, { .u_int = 240 } },
        { MP_QSTR_frame_width, MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_frame_height, MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_maximum_compatibility, MP_ARG_OBJ, { .u_obj = mp_const_false } }
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    self = m_new_obj_with_finaliser(ModPicoGraphics_obj_t);
    self->base.type = &ModPicoGraphics_type;

    bool status = false;
    int width = args[ARG_width].u_int;
    int height = args[ARG_height].u_int;
    int frame_width = args[ARG_frame_width].u_int == -1 ? width : args[ARG_frame_width].u_int;
    int frame_height = args[ARG_frame_height].u_int == -1 ? height : args[ARG_frame_height].u_int;
    bool maximum_compatability = mp_obj_is_true(args[ARG_maximum_compatibility].u_obj);

    if(frame_width < width || frame_height < height) {
        mp_raise_msg(&mp_type_RuntimeError, "PicoVision: Frame smaller than display!");
    }

    int pen_type = args[ARG_pen_type].u_int;

    // Create an instance of the graphics library and DV display driver
    switch((PicoGraphicsPenType)pen_type) {
        case PEN_DV_RGB888:
            self->graphics = m_new_class(PicoGraphics_PenDV_RGB888, frame_width, frame_height, dv_display);
            status = dv_display.init(width, height, DVDisplay::MODE_RGB888, frame_width, frame_height, maximum_compatability);
            break;
        case PEN_DV_RGB555:
            self->graphics = m_new_class(PicoGraphics_PenDV_RGB555, frame_width, frame_height, dv_display);
            status = dv_display.init(width, height, DVDisplay::MODE_RGB555, frame_width, frame_height, maximum_compatability);
            break;
        case PEN_DV_P5:
            self->graphics = m_new_class(PicoGraphics_PenDV_P5, frame_width, frame_height, dv_display);
            status = dv_display.init(width, height, DVDisplay::MODE_PALETTE, frame_width, frame_height, maximum_compatability);
            break;
        default:
            break;
    }

    if (!status) {
        mp_raise_msg(&mp_type_RuntimeError, "PicoVision: Unsupported Mode!");
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
mp_obj_t ModPicoGraphics_set_scroll_group_offset(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_scroll_group, ARG_x, ARG_y, ARG_wrap_x, ARG_wrap_y, ARG_wrap_x_to, ARG_wrap_y_to };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_scroll_group, MP_ARG_INT, { .u_int = 1 } },
        { MP_QSTR_x, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_y, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_wrap_x, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_wrap_y, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_wrap_x_to, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_wrap_y_to, MP_ARG_INT, { .u_int = 0 } },
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    dv_display.setup_scroll_group(
        Point(
            args[ARG_x].u_int,
            args[ARG_y].u_int
        ),
        args[ARG_scroll_group].u_int,
        args[ARG_wrap_x].u_int,
        args[ARG_wrap_y].u_int,
        args[ARG_wrap_x_to].u_int,
        args[ARG_wrap_y_to].u_int
    );

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_scroll_group_for_lines(size_t n_args, const mp_obj_t *args) {
    enum { ARG_self, ARG_scroll_index, ARG_min_y, ARG_max_y };

    dv_display.set_scroll_idx_for_lines(
        mp_obj_get_int(args[ARG_scroll_index]),
        mp_obj_get_int(args[ARG_min_y]),
        mp_obj_get_int(args[ARG_max_y]));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_load_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_filename, ARG_index, ARG_source };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_filename, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_index, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_source, MP_ARG_OBJ, {.u_obj = mp_const_none} }
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    int index = args[ARG_index].u_int;

    bool palette_lookups = self->graphics->pen_type != PicoGraphics::PEN_DV_P5;

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
            mp_printf(&mp_plat_print, "load_sprite: Not a PNG, loading as a PVS sprite.\n");
            mp_stream_read_exactly(fhandle, (uint8_t *)buf + sizeof(PNG_HEADER), fsize - sizeof(PNG_HEADER), &error);
            self->display->load_pvs_sprite(args[ARG_index].u_int, (uint32_t *)buf, fsize);
            m_free(buf);
            return mp_const_none;
        }

    } else {
        // It might be a buffer...
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[ARG_filename].u_obj, &bufinfo, MP_BUFFER_READ);

        if (strncmp((const char *)bufinfo.buf, PNG_HEADER, sizeof(PNG_HEADER)) != 0) {
            mp_printf(&mp_plat_print, "load_sprite: Not a PNG, loading as a PVS sprite.\n");
            self->display->load_pvs_sprite(args[ARG_index].u_int , (uint32_t*)bufinfo.buf, bufinfo.len);
            m_del(uint8_t, bufinfo.buf, bufinfo.len);

            return mp_const_none;
        }

        m_del(uint8_t, bufinfo.buf, bufinfo.len);
    }

    PNG *png = m_new_class(PNG);

    mp_obj_t file = args[ARG_filename].u_obj;

    mp_buffer_info_t buf;

    int status = -1;

    if(mp_obj_is_str_or_bytes(file)){
        GET_STR_DATA_LEN(file, str, str_len);

        status = png->open(
            (const char*)str,
            pngdec_open_callback,
            pngdec_close_callback,
            pngdec_read_callback,
            pngdec_seek_callback,
            palette_lookups ? PNGDrawSprite : PNGDrawSprite_Indexed);

    // Source is a buffer
    } else {
        mp_get_buffer_raise(file, &buf, MP_BUFFER_READ);

        status = png->openRAM((uint8_t *)buf.buf, buf.len, palette_lookups ? PNGDrawSprite : PNGDrawSprite_Indexed);
    }

    if(status != 0) mp_raise_msg(&mp_type_RuntimeError, "load_sprite: could not read file/buffer.");

    int width = png->getWidth();
    int height = png->getHeight();

    //mp_printf(&mp_plat_print, "load_sprite: Opened PNG %dx%d.\n", width, height);

    _PNG_decode_target *decode_target = m_new(_PNG_decode_target, 1);
    decode_target->target = nullptr;
    decode_target->source = {0, 0, width, height};

    if(mp_obj_is_type(args[ARG_source].u_obj, &mp_type_tuple)){
        mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(args[ARG_source].u_obj, mp_obj_tuple_t);

        if(tuple->len != 4) mp_raise_ValueError("load_sprite: source tuple must contain (x, y, w, h)");

        decode_target->source = {
            mp_obj_get_int(tuple->items[0]),
            mp_obj_get_int(tuple->items[1]),
            mp_obj_get_int(tuple->items[2]),
            mp_obj_get_int(tuple->items[3])
        };

        width = decode_target->source.w;
        height = decode_target->source.h;
    }

    // Create a buffer big enough to hold the data decoded from PNG by our draw callback

    // 16-bit RGB555 with 1-bit alpha vs 5-bit palette entries with 3-bit alpha
    size_t bytes_per_pixel = palette_lookups ? sizeof(uint16_t) : sizeof(uint8_t);
    decode_target->target = m_malloc(width * height * bytes_per_pixel);

    status = png->decode(decode_target, 0);

    // Close the file since we've opened it on-demand
    png->close();

    mp_obj_t result;

    if (index == -1) {
        mp_obj_t tuple[3] = {
            mp_obj_new_int(width),
            mp_obj_new_int(height),
            mp_obj_new_bytearray(width * height * bytes_per_pixel, decode_target->target)
        };
        result = mp_obj_new_tuple(3, tuple);
    } else {
        if (status == 0) {
            if(bytes_per_pixel == 1) {
                self->display->define_palette_sprite(args[ARG_index].u_int, width, height, (uint8_t *)(decode_target->target));
            } else {
                self->display->define_sprite(args[ARG_index].u_int, width, height, (uint16_t *)(decode_target->target));
            }
        }
        result = status == 0 ? mp_const_true : mp_const_false;
    }

    m_free(decode_target->target);
    m_free(decode_target);
    m_free(png);

    return result;
}

mp_obj_t ModPicoGraphics_load_animation(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_slot, ARG_data, ARG_frame_size, ARG_source };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_slot, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_frame_size, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_source, MP_ARG_OBJ, {.u_obj = mp_const_none} }
    };

    mp_buffer_info_t animation_data;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    int start_slot = args[ARG_slot].u_int;
    int slot = start_slot;

    // Frame size
    mp_obj_tuple_t *tuple_frame_size = MP_OBJ_TO_PTR2(args[ARG_frame_size].u_obj, mp_obj_tuple_t);

    if(tuple_frame_size->len != 2) mp_raise_ValueError("load_animation: frame_size tuple must contain (w, h)");

    int frame_w = mp_obj_get_int(tuple_frame_size->items[0]);
    int frame_h = mp_obj_get_int(tuple_frame_size->items[1]);

    // Animation data
    mp_obj_tuple_t *tuple_frame_data;
    bool cropped = false;

    // If the data supplied is a string, assume it's a filename and try to open it as a PNG
    if(mp_obj_is_str(args[ARG_data].u_obj)) {
        mp_obj_t pos_args[] = {
            args[ARG_self].u_obj,
            args[ARG_data].u_obj,
            mp_obj_new_int(-1),
            args[ARG_source].u_obj
        };
        tuple_frame_data = MP_OBJ_TO_PTR2(ModPicoGraphics_load_sprite(MP_ARRAY_SIZE(pos_args), &pos_args[0], (mp_map_t *)&mp_const_empty_map), mp_obj_tuple_t);
        cropped = true;
    } else {
        tuple_frame_data = MP_OBJ_TO_PTR2(args[ARG_data].u_obj, mp_obj_tuple_t);
    }

    if(tuple_frame_data->len != 3) mp_raise_ValueError("load_animation: data tuple must contain (w, h, data)");

    int tilesheet_w = mp_obj_get_int(tuple_frame_data->items[0]);
    int tilesheet_h = mp_obj_get_int(tuple_frame_data->items[1]);

    int source_x = 0;
    int source_y = 0;
    int source_w = tilesheet_w;
    int source_h = tilesheet_h;

    // Ignore the source rect if we've already passed it to ModPicoGraphics_load_sprite
    if(mp_obj_is_type(args[ARG_source].u_obj, &mp_type_tuple) && !cropped){
        mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(args[ARG_source].u_obj, mp_obj_tuple_t);

        if(tuple->len != 4) mp_raise_ValueError("load_animation: source tuple must contain (x, y, w, h)");

        source_x = mp_obj_get_int(tuple->items[0]);
        source_y = mp_obj_get_int(tuple->items[1]);
        source_w = mp_obj_get_int(tuple->items[2]);
        source_h = mp_obj_get_int(tuple->items[3]);
    }

    mp_get_buffer_raise(tuple_frame_data->items[2], &animation_data, MP_BUFFER_READ);

    int frames_x = source_w / frame_w;
    int frames_y = source_h / frame_h;

    if(self->graphics->pen_type == PicoGraphics::PEN_DV_P5) {

        uint8_t *buf = m_new(uint8_t, frame_w * frame_h);

        for(auto y = 0; y < frames_y; y++) {
            int o_y = (source_y + y * frame_h) * tilesheet_w;
            for(auto x = 0; x < frames_x; x++) {
                int o_x = source_x + x * frame_w;
                uint8_t *p = buf;
                uint8_t *data = (uint8_t *)animation_data.buf;
                data += o_x + o_y;
                for(auto fy = 0; fy < frame_h; fy++) {
                    for(auto fx = 0; fx < frame_w; fx++) {
                        *p++ = *data++;
                    }
                    // Advance to the next row
                    data += tilesheet_w - frame_w;
                }
                self->display->define_palette_sprite(slot++, frame_w, frame_h, buf);
            }
        }

    } else {

        uint16_t *buf = m_new(uint16_t, frame_w * frame_h);

        for(auto y = 0; y < frames_y; y++) {
            int o_y = (source_y + y * frame_h) * tilesheet_w;
            for(auto x = 0; x < frames_x; x++) {
                int o_x = source_x + x * frame_w;
                uint16_t *p = buf;
                uint16_t *data = (uint16_t *)animation_data.buf;
                data += o_x + o_y;
                for(auto fy = 0; fy < frame_h; fy++) {
                    for(auto fx = 0; fx < frame_w; fx++) {
                        *p++ = *data++;
                    }
                    data += tilesheet_w - frame_w;
                }
                self->display->define_sprite(slot++, frame_w, frame_h, buf);
            }
        }
    }

    mp_obj_t *tuple = m_new(mp_obj_t, frames_x * frames_y);
    for(auto x = 0; x < frames_x * frames_y; x++) {
        tuple[x] = mp_obj_new_int(start_slot + x);
    }
    return mp_obj_new_list(frames_x * frames_y, tuple);
}

mp_obj_t ModPicoGraphics_tilemap(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_tilemap, ARG_bounds, ARG_tile_data };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_tilemap, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_bounds, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_tile_data, MP_ARG_REQUIRED | MP_ARG_OBJ },
    };

    mp_buffer_info_t tilesheet_data;
    mp_buffer_info_t tilemap_data;

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, ModPicoGraphics_obj_t);

    // drawing region
    mp_obj_tuple_t *tuple_bounds = MP_OBJ_TO_PTR2(args[ARG_bounds].u_obj, mp_obj_tuple_t);

    if(tuple_bounds->len != 4) mp_raise_ValueError("tilemap: bounds tuple must contain (x, y, w, h)");

    int origin_x = mp_obj_get_int(tuple_bounds->items[0]);
    int origin_y = mp_obj_get_int(tuple_bounds->items[1]);
    int tiles_x = mp_obj_get_int(tuple_bounds->items[2]);
    int tiles_y = mp_obj_get_int(tuple_bounds->items[3]);

    // sprite data
    mp_obj_tuple_t *tuple_tile_data = MP_OBJ_TO_PTR2(args[ARG_tile_data].u_obj, mp_obj_tuple_t);

    if(tuple_tile_data->len != 3) mp_raise_ValueError("tilemap: tile_data tuple must contain (w, h, data)");

    int tilesheet_stride = mp_obj_get_int(tuple_tile_data->items[0]);
    mp_get_buffer_raise(tuple_tile_data->items[2], &tilesheet_data, MP_BUFFER_READ);

    mp_get_buffer_raise(args[ARG_tilemap].u_obj, &tilemap_data, MP_BUFFER_READ);

    uint8_t *tilemap = (uint8_t *)tilemap_data.buf;

    if(self->graphics->pen_type == PicoGraphics::PEN_DV_P5) {

        uint8_t *pixel_data = (uint8_t *)tilesheet_data.buf;

        for(auto y = 0; y < tiles_y * 16; y++) {
            int tile_y = y / 16 * tiles_x;
            for(auto x = 0; x < tiles_x * 16; x++) {
                int tile_x = x / 16;
                int tile_index = tile_x + tile_y;
                int tile = tilemap[tile_index];
                if(tile == 0) continue;
                tile--;
                tile &= 0b1111;

                int tilesheet_x = (tile & 0b11) * 16;
                int tilesheet_y = ((tile >> 2) & 0b11) * 16;

                int tile_pixel_x = (x & 0b1111) + tilesheet_x;
                int tile_pixel_y = (y & 0b1111) + tilesheet_y;

                int tilesheet_index = tile_pixel_y * tilesheet_stride + tile_pixel_x;

                uint8_t tile_pixel = pixel_data[tilesheet_index];

                if((tile_pixel & 0x01) == 0) continue;

                self->graphics->set_pen((tile_pixel >> 2) & 0x1f);

                self->graphics->pixel({
                    origin_x + x,
                    origin_y + y
                });
            }
        }

    } else {

        uint16_t *pixel_data = (uint16_t *)tilesheet_data.buf;

        for(auto y = 0; y < tiles_y * 16; y++) {
            int tile_y = y / 16 * tiles_x;
            for(auto x = 0; x < tiles_x * 16; x++) {
                int tile_x = x / 16;
                int tile_index = tile_x + tile_y;
                int tile = tilemap[tile_index];
                if(tile == 0) continue;
                tile--;
                tile &= 0b1111;

                int tilesheet_x = (tile & 0b11) * 16;
                int tilesheet_y = ((tile >> 2) & 0b11) * 16;

                int tile_pixel_x = (x & 0b1111) + tilesheet_x;
                int tile_pixel_y = (y & 0b1111) + tilesheet_y;

                int tilesheet_index = tile_pixel_y * tilesheet_stride + tile_pixel_x;

                RGB555 tile_pixel = pixel_data[tilesheet_index];

                if((tile_pixel & 0x8000) == 0) continue;

                uint8_t r, g, b;

                switch(self->graphics->pen_type) {
                    case PicoGraphics::PEN_DV_RGB888:
                        r = (tile_pixel & 0x7c00) >> 7;
                        g = (tile_pixel & 0x03e0) >> 2;
                        b = (tile_pixel & 0x001f) << 3;
                        self->graphics->set_pen(RGB(r, g, b).to_rgb888());
                        break;
                    case PicoGraphics::PEN_DV_RGB555:
                        self->graphics->set_pen((RGB555)tile_pixel);
                        break;
                    default:  // Non-dv pen types
                        break;
                };
                self->graphics->pixel({
                    origin_x + x,
                    origin_y + y
                });
            }

        }
    }

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_display_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_slot, ARG_sprite_index, ARG_x, ARG_y, ARG_blend_mode, ARG_v_scale };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_slot, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_sprite_index, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_blend_mode, MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_v_scale, MP_ARG_INT, {.u_int = 1} }
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    dv_display.set_sprite(args[ARG_slot].u_int, 
                          args[ARG_sprite_index].u_int,
                          {args[ARG_x].u_int, args[ARG_y].u_int},
                          (DVDisplay::SpriteBlendMode)args[ARG_blend_mode].u_int,
                          args[ARG_v_scale].u_int);

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

mp_obj_t ModPicoGraphics_set_bg(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_bg(mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_blend_mode(mp_obj_t self_in, mp_obj_t pen) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_blend_mode((BlendMode)mp_obj_get_int(pen));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_depth(mp_obj_t self_in, mp_obj_t depth) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->graphics->set_depth(mp_obj_get_int(depth));

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

mp_obj_t ModPicoGraphics_set_remote_palette(mp_obj_t self_in, mp_obj_t index) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->display->set_display_palette_index(mp_obj_get_int(index));

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_set_local_palette(mp_obj_t self_in, mp_obj_t index) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);

    self->display->set_local_palette_index(mp_obj_get_int(index));

    return mp_const_none;
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
    enum { ARG_self, ARG_text, ARG_x, ARG_y, ARG_wrap, ARG_scale, ARG_angle, ARG_spacing, ARG_fixed_width };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_y1, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_wordwrap, MP_ARG_INT, {.u_int = __INT32_MAX__} },  // Sort-of a fudge, but wide-enough to avoid wrapping on any display size
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_angle, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_fixed_width, MP_ARG_OBJ, {.u_obj = mp_const_false} },
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
    bool fixed_width = mp_obj_is_true(args[ARG_fixed_width].u_obj);

    self->graphics->text(t, Point(x, y), wrap, scale, angle, letter_spacing, fixed_width);

    return mp_const_none;
}

mp_obj_t ModPicoGraphics_measure_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_text, ARG_scale, ARG_spacing, ARG_fixed_width };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_scale, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_spacing, MP_ARG_INT, {.u_int = 1} },
        { MP_QSTR_fixed_width, MP_ARG_OBJ, {.u_obj = mp_const_false} },
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
    bool fixed_width = mp_obj_is_true(args[ARG_fixed_width].u_obj);

    int width = self->graphics->measure_text(t, scale, letter_spacing, fixed_width);

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

mp_obj_t ModPicoGraphics_loop(mp_obj_t self_in, mp_obj_t update, mp_obj_t render) {
    (void)self_in;
    /*
    TODO: Uh how do we typecheck a function?
    if(!mp_obj_is_type(update, &mp_type_function)) {
        mp_raise_TypeError("update(ticks_ms) must be a function.");
    }
    if(!mp_obj_is_type(render, &mp_type_function)) {
        mp_raise_TypeError("render(ticks_ms) must be a function.");
    }*/
    //ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    absolute_time_t t_start = get_absolute_time();
    mp_obj_t result;
    while(true) {
        uint32_t tick = absolute_time_diff_us(t_start, get_absolute_time()) / 1000;
        result = mp_call_function_1(update, mp_obj_new_int(tick));
        if (result == mp_const_false) break;
        dv_display.wait_for_flip();
        result = mp_call_function_1(render, mp_obj_new_int(tick));
        if (result == mp_const_false) break;
        dv_display.flip_async();
        MICROPY_EVENT_POLL_HOOK
    }
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_is_button_x_pressed(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    return self->display->is_button_x_pressed() ? mp_const_true : mp_const_false;
}

mp_obj_t ModPicoGraphics_is_button_a_pressed(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    return self->display->is_button_a_pressed() ? mp_const_true : mp_const_false;
}

mp_obj_t ModPicoGraphics_get_gpu_io_value(mp_obj_t self_in, mp_obj_t pin) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    if (pin_num >= 0 && pin_num < 8) {
        // "High" IO number
        bool value = (self->display->get_gpio_hi() & (1 << pin_num)) != 0;
        return value ? mp_const_true : mp_const_false;
    }
    else if (pin_num >= 23 && pin_num < 30) {
        // "Low" IO number
        bool value = (self->display->get_gpio() & (1 << pin_num)) != 0;
        return value ? mp_const_true : mp_const_false;
    }

    mp_raise_ValueError("get_gpu_io_value(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_set_gpu_io_value(mp_obj_t self_in, mp_obj_t pin, mp_obj_t value) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    if (pin_num >= 0 && pin_num < 8) {
        // "High" IO number
        bool on = mp_obj_is_true(value);
        self->display->set_gpio_hi(pin_num, on);
        return mp_const_none;
    }
    else if (pin_num == 29) {
        uint8_t level;
        if (mp_obj_is_bool(value)) level = mp_obj_is_true(value) ? 255 : 0;
        else if (mp_obj_is_int(value)) level = mp_obj_get_int(value);
        else if (mp_obj_is_float(value)) level = mp_obj_get_float(value) * 255;
        else mp_raise_ValueError("set_gpu_io_value(): invalid value type");
        self->display->set_gpio_29_value(level);
        return mp_const_none;
    }

    mp_raise_ValueError("set_gpu_io_value(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_set_gpu_io_output_enable(mp_obj_t self_in, mp_obj_t pin, mp_obj_t enable) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    bool output = mp_obj_is_true(enable);
    if (pin_num >= 0 && pin_num < 8) {
        // "High" IO number
        self->display->set_gpio_hi_dir(pin_num, output);
        return mp_const_none;
    }
    else if (pin_num == 29) {
        self->display->set_gpio_29_dir(output);
        return mp_const_none;
    }

    mp_raise_ValueError("set_gpu_io_output_enable(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_set_gpu_io_pull_up(mp_obj_t self_in, mp_obj_t pin, mp_obj_t enable) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    bool on = mp_obj_is_true(enable);
    if (pin_num >= 0 && pin_num < 8) {
        // "High" IO number
        self->display->set_gpio_hi_pull_up(pin_num, on);
        return mp_const_none;
    }
    else if (pin_num == 29) {
        self->display->set_gpio_29_pull_up(on);
        return mp_const_none;
    }

    mp_raise_ValueError("set_gpu_io_pull_up(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_set_gpu_io_pull_down(mp_obj_t self_in, mp_obj_t pin, mp_obj_t enable) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    bool on = mp_obj_is_true(enable);
    if (pin_num >= 0 && pin_num < 8) {
        // "High" IO number
        self->display->set_gpio_hi_pull_down(pin_num, on);
        return mp_const_none;
    }
    else if (pin_num == 29) {
        self->display->set_gpio_29_pull_down(on);
        return mp_const_none;
    }

    mp_raise_ValueError("set_gpu_io_pull_down(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_set_gpu_io_adc_enable(mp_obj_t self_in, mp_obj_t pin, mp_obj_t enable) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    bool on = mp_obj_is_true(enable);
    if (pin_num == 29) {
        if (on) self->display->enable_gpio_29_adc();
        else self->display->set_gpio_29_dir(false);
        return mp_const_none;
    }

    mp_raise_ValueError("set_gpu_io_adc_enable(): invalid pin number");
    return mp_const_false;
}

mp_obj_t ModPicoGraphics_get_gpu_io_adc_voltage(mp_obj_t self_in, mp_obj_t pin) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    
    int pin_num = mp_obj_get_int(pin);
    if (pin_num == 29) {
        return mp_obj_new_float(self->display->get_gpio_29_adc());
    }

    mp_raise_ValueError("get_gpu_io_adc_voltage(): invalid pin number");
    return mp_const_none;
}

mp_obj_t ModPicoGraphics_get_gpu_temp(mp_obj_t self_in) {
    ModPicoGraphics_obj_t *self = MP_OBJ_TO_PTR2(self_in, ModPicoGraphics_obj_t);
    return mp_obj_new_float(self->display->get_gpu_temp());
}

mp_obj_t ModPicoGraphics_get_i2c(mp_obj_t self_in) {
    _PimoroniI2C_obj_t* i2c_obj = m_new_obj(_PimoroniI2C_obj_t);
    i2c_obj->base.type = &PimoroniI2C_type;

    i2c_obj->i2c = &dv_i2c;

    return i2c_obj;
}
}
