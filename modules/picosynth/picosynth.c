#include "picosynth.h"


/***** Methods *****/
MP_DEFINE_CONST_FUN_OBJ_1(Channel___del___obj, Channel___del__);
MP_DEFINE_CONST_FUN_OBJ_KW(Channel_configure_obj, 1, Channel_configure);
MP_DEFINE_CONST_FUN_OBJ_1(Channel_restore_obj, Channel_restore);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_waveforms_obj, 1, 2, Channel_waveforms);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_frequency_obj, 1, 2, Channel_frequency);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_volume_obj, 1, 2, Channel_volume);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_attack_duration_obj, 1, 2, Channel_attack_duration);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_decay_duration_obj, 1, 2, Channel_decay_duration);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_sustain_level_obj, 1, 2, Channel_sustain_level);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_release_duration_obj, 1, 2, Channel_release_duration);
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(Channel_pulse_width_obj, 1, 2, Channel_pulse_width);
MP_DEFINE_CONST_FUN_OBJ_1(Channel_trigger_attack_obj, Channel_trigger_attack);
MP_DEFINE_CONST_FUN_OBJ_1(Channel_trigger_release_obj, Channel_trigger_release);
MP_DEFINE_CONST_FUN_OBJ_KW(Channel_play_tone_obj, 2, Channel_play_tone);

MP_DEFINE_CONST_FUN_OBJ_1(PicoSynth___del___obj, PicoSynth___del__);
MP_DEFINE_CONST_FUN_OBJ_2(PicoSynth_set_volume_obj, PicoSynth_set_volume);
MP_DEFINE_CONST_FUN_OBJ_1(PicoSynth_get_volume_obj, PicoSynth_get_volume);
MP_DEFINE_CONST_FUN_OBJ_2(PicoSynth_adjust_volume_obj, PicoSynth_adjust_volume);
MP_DEFINE_CONST_FUN_OBJ_2(PicoSynth_play_sample_obj, PicoSynth_play_sample);
MP_DEFINE_CONST_FUN_OBJ_1(PicoSynth_play_synth_obj, PicoSynth_play_synth);
MP_DEFINE_CONST_FUN_OBJ_1(PicoSynth_stop_playing_obj, PicoSynth_stop_playing);
MP_DEFINE_CONST_FUN_OBJ_2(PicoSynth_synth_channel_obj, PicoSynth_synth_channel);

/***** Binding of Methods *****/
STATIC const mp_rom_map_elem_t Channel_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&Channel___del___obj) },
    { MP_ROM_QSTR(MP_QSTR_configure), MP_ROM_PTR(&Channel_configure_obj) },
    { MP_ROM_QSTR(MP_QSTR_restore), MP_ROM_PTR(&Channel_restore_obj) },
    { MP_ROM_QSTR(MP_QSTR_waveforms), MP_ROM_PTR(&Channel_waveforms_obj) },
    { MP_ROM_QSTR(MP_QSTR_frequency), MP_ROM_PTR(&Channel_frequency_obj) },
    { MP_ROM_QSTR(MP_QSTR_volume), MP_ROM_PTR(&Channel_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_attack_duration), MP_ROM_PTR(&Channel_attack_duration_obj) },
    { MP_ROM_QSTR(MP_QSTR_decay_duration), MP_ROM_PTR(&Channel_decay_duration_obj) },
    { MP_ROM_QSTR(MP_QSTR_sustain_level), MP_ROM_PTR(&Channel_sustain_level_obj) },
    { MP_ROM_QSTR(MP_QSTR_release_duration), MP_ROM_PTR(&Channel_release_duration_obj) },
    { MP_ROM_QSTR(MP_QSTR_pulse_width), MP_ROM_PTR(&Channel_pulse_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_trigger_attack), MP_ROM_PTR(&Channel_trigger_attack_obj) },
    { MP_ROM_QSTR(MP_QSTR_trigger_release), MP_ROM_PTR(&Channel_trigger_release_obj) },
    { MP_ROM_QSTR(MP_QSTR_play_tone), MP_ROM_PTR(&Channel_play_tone_obj) },

    { MP_ROM_QSTR(MP_QSTR_NOISE), MP_ROM_INT(128) },
    { MP_ROM_QSTR(MP_QSTR_SQUARE), MP_ROM_INT(64) },
    { MP_ROM_QSTR(MP_QSTR_SAW), MP_ROM_INT(32) },
    { MP_ROM_QSTR(MP_QSTR_TRIANGLE), MP_ROM_INT(16) },
    { MP_ROM_QSTR(MP_QSTR_SINE), MP_ROM_INT(8) },
    { MP_ROM_QSTR(MP_QSTR_WAVE), MP_ROM_INT(1) },
};

STATIC const mp_rom_map_elem_t PicoSynth_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&PicoSynth___del___obj) },
    { MP_ROM_QSTR(MP_QSTR_set_volume), MP_ROM_PTR(&PicoSynth_set_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_volume), MP_ROM_PTR(&PicoSynth_get_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_adjust_volume), MP_ROM_PTR(&PicoSynth_adjust_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_play_sample), MP_ROM_PTR(&PicoSynth_play_sample_obj) },
    { MP_ROM_QSTR(MP_QSTR_play), MP_ROM_PTR(&PicoSynth_play_synth_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&PicoSynth_stop_playing_obj) },
    { MP_ROM_QSTR(MP_QSTR_channel), MP_ROM_PTR(&PicoSynth_synth_channel_obj) },

};

STATIC MP_DEFINE_CONST_DICT(Channel_locals_dict, Channel_locals_dict_table);
STATIC MP_DEFINE_CONST_DICT(PicoSynth_locals_dict, PicoSynth_locals_dict_table);

/***** Class Definition *****/
#ifdef MP_DEFINE_CONST_OBJ_TYPE
MP_DEFINE_CONST_OBJ_TYPE(
    Channel_type,
    MP_QSTR_Channel,
    MP_TYPE_FLAG_NONE,
    make_new, Channel_make_new,
    print, Channel_print,
    locals_dict, (mp_obj_dict_t*)&Channel_locals_dict
);

MP_DEFINE_CONST_OBJ_TYPE(
    PicoSynth_type,
    MP_QSTR_PicoSynth,
    MP_TYPE_FLAG_NONE,
    make_new, PicoSynth_make_new,
    print, PicoSynth_print,
    locals_dict, (mp_obj_dict_t*)&PicoSynth_locals_dict
);
#else
const mp_obj_type_t Channel_type = {
    { &mp_type_type },
    .name = MP_QSTR_Channel,
    .print = Channel_print,
    .make_new = Channel_make_new,
    .locals_dict = (mp_obj_dict_t*)&Channel_locals_dict,
};

const mp_obj_type_t PicoSynth_type = {
    { &mp_type_type },
    .name = MP_QSTR_PicoSynth,
    .print = PicoSynth_print,
    .make_new = PicoSynth_make_new,
    .locals_dict = (mp_obj_dict_t*)&PicoSynth_locals_dict,
};
#endif

/***** Globals Table *****/
STATIC const mp_map_elem_t picosynth_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_picosynth) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Channel), (mp_obj_t)&Channel_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PicoSynth), (mp_obj_t)&PicoSynth_type },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_picosynth_globals, picosynth_globals_table);

/***** Module Definition *****/
const mp_obj_module_t picosynth_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_picosynth_globals,
};
#if MICROPY_VERSION <= 70144
MP_REGISTER_MODULE(MP_QSTR_picosynth, picosynth_user_cmodule, MODULE_PICOSYNTH_ENABLED);
#else
MP_REGISTER_MODULE(MP_QSTR_picosynth, picosynth_user_cmodule);
#endif