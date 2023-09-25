// Include MicroPython API.
#include "py/runtime.h"

/***** Extern of Class Definition *****/
extern const mp_obj_type_t Channel_type;
extern const mp_obj_type_t PicoSynth_type;

/***** Extern of Class Methods *****/
extern void Channel_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);
extern mp_obj_t Channel_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args);
extern mp_obj_t Channel___del__(mp_obj_t self_in);
extern mp_obj_t Channel_configure(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t Channel_restore(mp_obj_t self_in);
extern mp_obj_t Channel_waveforms(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_frequency(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_volume(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_attack_duration(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_decay_duration(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_sustain_level(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_release_duration(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_pulse_width(size_t n_args, const mp_obj_t *args);
extern mp_obj_t Channel_trigger_attack(mp_obj_t self_in);
extern mp_obj_t Channel_trigger_release(mp_obj_t self_in);
extern mp_obj_t Channel_play_tone(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
extern mp_obj_t Channel_stop_playing(mp_obj_t self_in);

extern void PicoSynth_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind);
extern mp_obj_t PicoSynth_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args);
extern mp_obj_t PicoSynth___del__(mp_obj_t self_in);

extern mp_obj_t PicoSynth_set_volume(mp_obj_t self_in, mp_obj_t value);
extern mp_obj_t PicoSynth_get_volume(mp_obj_t self_in);
extern mp_obj_t PicoSynth_adjust_volume(mp_obj_t self_in, mp_obj_t delta);

extern mp_obj_t PicoSynth_play_sample(mp_obj_t self_in, mp_obj_t data);
extern mp_obj_t PicoSynth_play_synth(mp_obj_t self_in);
extern mp_obj_t PicoSynth_stop_playing(mp_obj_t self_in);

extern mp_obj_t PicoSynth_synth_channel(mp_obj_t self_in, mp_obj_t channel_in);