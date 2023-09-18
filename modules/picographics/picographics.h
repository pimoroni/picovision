#include "py/runtime.h"
#include "py/objstr.h"

enum PicoGraphicsPenType {
    PEN_1BIT = 0,
    PEN_3BIT,
    PEN_P2,
    PEN_P4,
    PEN_P8,
    PEN_RGB332,
    PEN_RGB565,
    PEN_RGB888,
    PEN_INKY7,
    PEN_DV_RGB888,
    PEN_DV_RGB555,
    PEN_DV_P5
};

enum PicoGraphicsBusType {
    BUS_I2C,
    BUS_SPI,
    BUS_PARALLEL,
    BUS_PIO
};

// Type
extern const mp_obj_type_t ModPicoGraphics_type;

// Module functions
extern mp_obj_t ModPicoGraphics_get_required_buffer_size(mp_obj_t display_in, mp_obj_t pen_type_in);

// DV Display specific functions
extern mp_obj_t ModPicoGraphics_set_display_offset(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_set_scroll_index_for_lines(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_tilemap(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_load_animation(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);

// Class methods
extern mp_obj_t ModPicoGraphics_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args);

extern mp_obj_t ModPicoGraphics_update(mp_obj_t self_in);

// Palette management
extern mp_obj_t ModPicoGraphics_update_pen(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_reset_pen(mp_obj_t self_in, mp_obj_t pen);
extern mp_obj_t ModPicoGraphics_set_palette(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_hsv_to_rgb(size_t n_args, const mp_obj_t *args);

extern mp_obj_t ModPicoGraphics_set_remote_palette(mp_obj_t self_in, mp_obj_t index);
extern mp_obj_t ModPicoGraphics_set_local_palette(mp_obj_t self_in, mp_obj_t index);

// Pen
extern mp_obj_t ModPicoGraphics_set_pen(mp_obj_t self_in, mp_obj_t pen);
extern mp_obj_t ModPicoGraphics_create_pen(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_create_pen_hsv(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_set_thickness(mp_obj_t self_in, mp_obj_t thickness);

// Alpha Blending
extern mp_obj_t ModPicoGraphics_set_bg(mp_obj_t self_in, mp_obj_t pen);
extern mp_obj_t ModPicoGraphics_set_blend_mode(mp_obj_t self_in, mp_obj_t pen);

// Depth
extern mp_obj_t ModPicoGraphics_set_depth(mp_obj_t self_in, mp_obj_t depth);

// Primitives
extern mp_obj_t ModPicoGraphics_set_clip(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_remove_clip(mp_obj_t self_in);
extern mp_obj_t ModPicoGraphics_clear(mp_obj_t self_in);
extern mp_obj_t ModPicoGraphics_pixel(mp_obj_t self_in, mp_obj_t x, mp_obj_t y);
extern mp_obj_t ModPicoGraphics_pixel_span(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_rectangle(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_circle(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_character(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_measure_text(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_polygon(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_triangle(size_t n_args, const mp_obj_t *args);
extern mp_obj_t ModPicoGraphics_line(size_t n_args, const mp_obj_t *args);

// Sprites
extern mp_obj_t ModPicoGraphics_load_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_display_sprite(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t ModPicoGraphics_clear_sprite(mp_obj_t self_in, mp_obj_t slot);

// Utility
extern mp_obj_t ModPicoGraphics_set_font(mp_obj_t self_in, mp_obj_t font);
extern mp_obj_t ModPicoGraphics_get_bounds(mp_obj_t self_in);
extern mp_obj_t ModPicoGraphics_set_framebuffer(mp_obj_t self_in, mp_obj_t framebuffer);

extern mp_int_t ModPicoGraphics_get_framebuffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags);

extern mp_obj_t ModPicoGraphics_get_i2c(mp_obj_t self_in);

extern mp_obj_t ModPicoGraphics__del__(mp_obj_t self_in);

// IO IO
extern mp_obj_t ModPicoGraphics_is_button_x_pressed(mp_obj_t self_in);
extern mp_obj_t ModPicoGraphics_is_button_a_pressed(mp_obj_t self_in);
