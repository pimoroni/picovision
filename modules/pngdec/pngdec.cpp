#include "lib/PNGdec.h"

#include "micropython/modules/util.hpp"
#include "drivers/dv_display/dv_display.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

using namespace pimoroni;

extern "C" {
#include "pngdec.h"
#include "micropython/modules/picographics/picographics.h"
#include "py/stream.h"
#include "py/reader.h"
#include "extmod/vfs.h"

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
    ModPicoGraphics_obj_t *graphics;
} _PNG_obj_t;


static uint8_t current_flags = 0;
static Point current_position = {0, 0};

enum FLAGS : uint8_t {
    FLAG_NO_DITHER = 1u
};

void *pngdec_open_callback(const char *filename, int32_t *size) {
    mp_obj_t fn = mp_obj_new_str(filename, (mp_uint_t)strlen(filename));

    mp_obj_t args[2] = {
        fn,
        MP_OBJ_NEW_QSTR(MP_QSTR_r),
    };

    // Stat the file to get its size
    // example tuple response: (32768, 0, 0, 0, 0, 0, 5153, 1654709815, 1654709815, 1654709815)
    mp_obj_t stat = mp_vfs_stat(fn);
    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR2(stat, mp_obj_tuple_t);
    *size = mp_obj_get_int(tuple->items[6]);

    mp_obj_t fhandle = mp_vfs_open(MP_ARRAY_SIZE(args), &args[0], (mp_map_t *)&mp_const_empty_map);

    return (void *)fhandle;
}

void pngdec_close_callback(void *handle) {
    mp_stream_close((mp_obj_t)handle);
}

int32_t pngdec_read_callback(PNGFILE *png, uint8_t *p, int32_t c) {
    mp_obj_t fhandle = png->fHandle;
    int error;
    return mp_stream_read_exactly(fhandle, p, c, &error);
}

// Re-implementation of stream.c/STATIC mp_obj_t stream_seek(size_t n_args, const mp_obj_t *args)
int32_t pngdec_seek_callback(PNGFILE *png, int32_t p) {
    mp_obj_t fhandle = png->fHandle;
    struct mp_stream_seek_t seek_s;
    seek_s.offset = p;
    seek_s.whence = SEEK_SET;

    const mp_stream_p_t *stream_p = mp_get_stream(fhandle);

    int error;
    mp_uint_t res = stream_p->ioctl(fhandle, MP_STREAM_SEEK, (mp_uint_t)(uintptr_t)&seek_s, &error);
    if (res == MP_STREAM_ERROR) {
        mp_raise_OSError(error);
    }

    return seek_s.offset;
}

void PNGDraw(PNGDRAW *pDraw) {
#ifdef MICROPY_EVENT_POLL_HOOK
MICROPY_EVENT_POLL_HOOK
#endif
    PicoGraphics *current_graphics = (PicoGraphics *)pDraw->pUser;
    // "pixel" is slow and clipped,
    // guaranteeing we wont draw png data out of the framebuffer..
    // Can we clip beforehand and make this faster?
    int y = pDraw->y;

    // TODO we need to handle PicoGraphics palette pen types, plus grayscale and gray alpha PNG modes
    // For DV P5 we could copy over the raw palette values (p & 0b00011111) and assume the user has prepped their palette accordingly
    // also dither, maybe?

    //mp_printf(&mp_plat_print, "Drawing scanline at %d, %dbpp, type: %d, width: %d pitch: %d alpha: %d\n", y, pDraw->iBpp, pDraw->iPixelType, pDraw->iWidth, pDraw->iPitch, pDraw->iHasAlpha);
    uint8_t *pixel = (uint8_t *)pDraw->pPixels;
    if(pDraw->iPixelType == PNG_PIXEL_TRUECOLOR ) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint8_t r = *pixel++;
            uint8_t g = *pixel++;
            uint8_t b = *pixel++;
            current_graphics->set_pen(r, g, b);
            current_graphics->pixel({current_position.x + x, current_position.y + y});
        }
    } else if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
        for(int x = 0; x < pDraw->iWidth; x++) {
            uint8_t r = *pixel++;
            uint8_t g = *pixel++;
            uint8_t b = *pixel++;
            uint8_t a = *pixel++;
            if (a) {
                current_graphics->set_pen(r, g, b);
                current_graphics->pixel({current_position.x + x, current_position.y + y});
            }
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
            uint8_t r = pDraw->pPalette[(i * 3) + 0];
            uint8_t g = pDraw->pPalette[(i * 3) + 1];
            uint8_t b = pDraw->pPalette[(i * 3) + 2];
            uint8_t a = pDraw->iHasAlpha ? pDraw->pPalette[768 + i] : 1;
            if (a) {
                current_graphics->set_pen(r, g, b);
                current_graphics->pixel({current_position.x + x, current_position.y + y});
            }
        }
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

mp_obj_t _PNG_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_picographics
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_picographics, MP_ARG_REQUIRED | MP_ARG_OBJ },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if(!MP_OBJ_IS_TYPE(args[ARG_picographics].u_obj, &ModPicoGraphics_type)) mp_raise_ValueError(MP_ERROR_TEXT("PicoGraphics Object Required"));

    _PNG_obj_t *self = m_new_obj_with_finaliser(_PNG_obj_t);
    self->base.type = &PNG_type;
    self->png = m_new_class(PNG);
    self->graphics = (ModPicoGraphics_obj_t *)MP_OBJ_TO_PTR(args[ARG_picographics].u_obj);

    return self;
}

mp_obj_t _PNG_del(mp_obj_t self_in) {
    _PNG_obj_t *self = MP_OBJ_TO_PTR2(self_in, _PNG_obj_t);
    self->png->close();
    return mp_const_none;
}

// open_FILE
mp_obj_t _PNG_openFILE(mp_obj_t self_in, mp_obj_t filename) {
    _PNG_obj_t *self = MP_OBJ_TO_PTR2(self_in, _PNG_obj_t);

    // TODO Check for valid filename, and maybe that file exists?

    self->file = filename;

    return mp_const_true;
}

// open_RAM
mp_obj_t _PNG_openRAM(mp_obj_t self_in, mp_obj_t buffer) {
    _PNG_obj_t *self = MP_OBJ_TO_PTR2(self_in, _PNG_obj_t);

    // TODO Check for valid buffer

    self->file = buffer;

    return mp_const_true;
}

// decode
mp_obj_t _PNG_decode(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_x, ARG_y, ARG_scale, ARG_dither };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_x, MP_ARG_INT, {.u_int = 0}  },
        { MP_QSTR_y, MP_ARG_INT, {.u_int = 0}  },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_dither, MP_ARG_OBJ, {.u_obj = mp_const_true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    _PNG_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, _PNG_obj_t);

    // TODO: Implement integer 1x/2x/3x scaling by repeating pixels?
    //int f = args[ARG_scale].u_int;

    current_flags = args[ARG_dither].u_obj == mp_const_false ? FLAG_NO_DITHER : 0;

    current_position = {args[ARG_x].u_int, args[ARG_y].u_int};

    // Just-in-time open of the filename/buffer we stored in self->file via open_RAM or open_file

    // Source is a filename
    int result = -1;

    if(mp_obj_is_str_or_bytes(self->file)){
        GET_STR_DATA_LEN(self->file, str, str_len);

        result = self->png->open(
            (const char*)str,
            pngdec_open_callback,
            pngdec_close_callback,
            pngdec_read_callback,
            pngdec_seek_callback,
            PNGDraw);

    // Source is a buffer
    } else {
        mp_get_buffer_raise(self->file, &self->buf, MP_BUFFER_READ);

        result = self->png->openRAM((uint8_t *)self->buf.buf, self->buf.len, PNGDraw);
    }

    //self->png->setBuffer((uint8_t *)m_malloc(self->png->getBufferSize()));

    if(result != 0) mp_raise_msg(&mp_type_RuntimeError, "PNG: could not read file/buffer.");

    // Force a specific data output type to best match our PicoGraphics buffer
    switch(self->graphics->graphics->pen_type) {
        case PicoGraphics::PEN_RGB332:
        case PicoGraphics::PEN_RGB565:
        case PicoGraphics::PEN_RGB888:
        case PicoGraphics::PEN_P8:
        case PicoGraphics::PEN_P4:
        case PicoGraphics::PEN_3BIT:
        case PicoGraphics::PEN_INKY7:
        case PicoGraphics::PEN_DV_RGB888:
        case PicoGraphics::PEN_DV_RGB555:
            //self->png->setPixelType(RGB565_BIG_ENDIAN);
            break;
        // TODO 2-bit is currently unsupported
        case PicoGraphics::PEN_DV_P5:
        case PicoGraphics::PEN_P2:
        case PicoGraphics::PEN_1BIT:
            //self->png->setPixelType(EIGHT_BIT_GRAYSCALE);
            break;
    }

    // We need to store a pointer to the PicoGraphics surface
    //self->png->setUserPointer((void *)self->graphics->graphics);

    result = self->png->decode((void *)self->graphics->graphics, 0);

    current_flags = 0;

    current_position = {0, 0};

    // Close the file since we've opened it on-demand
    self->png->close();

    //m_free(self->png->getBuffer());

    return result == 1 ? mp_const_true : mp_const_false;
}

// decode
mp_obj_t _PNG_decode_as_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_index, ARG_scale, ARG_dither };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_index, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_dither, MP_ARG_OBJ, {.u_obj = mp_const_true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    _PNG_obj_t *self = MP_OBJ_TO_PTR2(args[ARG_self].u_obj, _PNG_obj_t);

    // TODO: Implement integer 1x/2x/3x scaling by repeating pixels?
    //int f = args[ARG_scale].u_int;

    current_flags = args[ARG_dither].u_obj == mp_const_false ? FLAG_NO_DITHER : 0;

    // Just-in-time open of the filename/buffer we stored in self->file via open_RAM or open_file

    // Source is a filename
    int result = -1;

    if(mp_obj_is_str_or_bytes(self->file)){
        GET_STR_DATA_LEN(self->file, str, str_len);

        result = self->png->open(
            (const char*)str,
            pngdec_open_callback,
            pngdec_close_callback,
            pngdec_read_callback,
            pngdec_seek_callback,
            PNGDrawSprite);

    // Source is a buffer
    } else {
        mp_get_buffer_raise(self->file, &self->buf, MP_BUFFER_READ);

        result = self->png->openRAM((uint8_t *)self->buf.buf, self->buf.len, PNGDraw);
    }

    //self->png->setBuffer((uint8_t *)m_malloc(self->png->getBufferSize()));

    if(result != 0) mp_raise_msg(&mp_type_RuntimeError, "PNG: could not read file/buffer.");

    uint16_t* buffer = (uint16_t*)m_malloc(self->png->getWidth() * self->png->getHeight() * sizeof(uint16_t));

    result = self->png->decode((void *)buffer, 0);

    self->graphics->display->define_sprite(args[ARG_index].u_int, self->png->getWidth(), self->png->getHeight(), buffer);

    m_free(buffer);

    current_flags = 0;

    // Close the file since we've opened it on-demand
    self->png->close();

    //m_free(self->png->getBuffer());

    return result == 1 ? mp_const_true : mp_const_false;
}

// get_width
mp_obj_t _PNG_getWidth(mp_obj_t self_in) {
    _PNG_obj_t *self = MP_OBJ_TO_PTR2(self_in, _PNG_obj_t);
    return mp_obj_new_int(self->png->getWidth());
}

// get_height
mp_obj_t _PNG_getHeight(mp_obj_t self_in) {
    _PNG_obj_t *self = MP_OBJ_TO_PTR2(self_in, _PNG_obj_t);
    return mp_obj_new_int(self->png->getHeight());
}

}
