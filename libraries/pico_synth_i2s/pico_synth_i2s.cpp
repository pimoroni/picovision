#include <math.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"


#ifndef NO_QSTR
#include "pico_synth_i2s.pio.h"
#endif

#include "pico_synth_i2s.hpp"


static uint32_t audio_dma_channel;

namespace pimoroni {

  PicoSynth_I2S* PicoSynth_I2S::picosynth = nullptr;
  PIO PicoSynth_I2S::audio_pio = pio0;
  uint PicoSynth_I2S::audio_sm = 0;
  uint PicoSynth_I2S::audio_sm_offset = 0;

  // once the dma transfer of the scanline is complete we move to the
  // next scanline (or quit if we're finished)
  void __isr PicoSynth_I2S::dma_complete() {
    if(picosynth != nullptr && dma_channel_get_irq0_status(audio_dma_channel)) {
      picosynth->next_audio_sequence();
    }
  }

  PicoSynth_I2S::~PicoSynth_I2S() {
    if(picosynth == this) {
      partial_teardown();

      dma_channel_unclaim(audio_dma_channel); // This works now the teardown behaves correctly
      pio_sm_unclaim(audio_pio, audio_sm);
      pio_remove_program(audio_pio, &audio_i2s_program, audio_sm_offset);
      irq_remove_handler(DMA_IRQ_0, dma_complete);

      picosynth = nullptr;
    }
  }

  void PicoSynth_I2S::partial_teardown() {
    // Stop the audio SM
    pio_sm_set_enabled(audio_pio, audio_sm, false);

    // Reset the I2S pins to avoid popping when audio is suddenly stopped
    const uint pins_to_clear = 1 << i2s_data | 1 << i2s_bclk | 1 << i2s_lrclk;
    pio_sm_set_pins_with_mask(audio_pio, audio_sm, 0, pins_to_clear);

    // Abort any in-progress DMA transfer
    dma_safe_abort(audio_dma_channel);
  }

  void PicoSynth_I2S::init() {

    if(picosynth != nullptr) {
      // Tear down the old GU instance's hardware resources
      partial_teardown();
    }

    // setup audio pio program
    audio_pio = pio0;
    if(picosynth == nullptr) {
      audio_sm = pio_claim_unused_sm(audio_pio, true);
      audio_sm_offset = pio_add_program(audio_pio, &audio_i2s_program);
    }

    pio_gpio_init(audio_pio, i2s_data);
    pio_gpio_init(audio_pio, i2s_bclk);
    pio_gpio_init(audio_pio, i2s_lrclk);

    audio_i2s_program_init(audio_pio, audio_sm, audio_sm_offset, i2s_data, i2s_bclk);
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    uint32_t divider = system_clock_frequency * 4 / SYSTEM_FREQ; // avoid arithmetic overflow
    pio_sm_set_clkdiv_int_frac(audio_pio, audio_sm, divider >> 8u, divider & 0xffu);

    audio_dma_channel = dma_claim_unused_channel(true);
    dma_channel_config audio_config = dma_channel_get_default_config(audio_dma_channel);
    channel_config_set_transfer_data_size(&audio_config, DMA_SIZE_16);
    //channel_config_set_bswap(&audio_config, false); // byte swap to reverse little endian
    channel_config_set_dreq(&audio_config, pio_get_dreq(audio_pio, audio_sm, true));
    dma_channel_configure(audio_dma_channel, &audio_config, &audio_pio->txf[audio_sm], NULL, 0, false);

    dma_channel_set_irq0_enabled(audio_dma_channel, true);

    if(picosynth == nullptr) {
      irq_add_shared_handler(DMA_IRQ_0, dma_complete, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
      irq_set_enabled(DMA_IRQ_0, true);
    }

    picosynth = this;
  }

  void PicoSynth_I2S::dma_safe_abort(uint channel) {
    // Tear down the DMA channel.
    // This is copied from: https://github.com/raspberrypi/pico-sdk/pull/744/commits/5e0e8004dd790f0155426e6689a66e08a83cd9fc
    uint32_t irq0_save = dma_hw->inte0 & (1u << channel);
    hw_clear_bits(&dma_hw->inte0, irq0_save);

    dma_hw->abort = 1u << channel;

    // To fence off on in-flight transfers, the BUSY bit should be polled
    // rather than the ABORT bit, because the ABORT bit can clear prematurely.
    while (dma_hw->ch[channel].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) tight_loop_contents();

    // Clear the interrupt (if any) and restore the interrupt masks.
    dma_hw->ints0 = 1u << channel;
    hw_set_bits(&dma_hw->inte0, irq0_save);
  }

  void PicoSynth_I2S::play_sample(uint8_t *data, uint32_t length) {
    stop_playing();

    if(picosynth == this) {
      // Restart the audio SM and start a new DMA transfer
      pio_sm_set_enabled(audio_pio, audio_sm, true);
      dma_channel_transfer_from_buffer_now(audio_dma_channel, data, length / 2);
      play_mode = PLAYING_BUFFER;
    }
  }

  void PicoSynth_I2S::play_synth() {
    if(play_mode != PLAYING_SYNTH) {
      stop_playing();
    }

    if(picosynth == this && play_mode == NOT_PLAYING) {
      // Nothing is playing, so we can set up the first buffer straight away
      current_buffer = 0;

      populate_next_synth();

      // Restart the audio SM and start a new DMA transfer
      pio_sm_set_enabled(audio_pio, audio_sm, true);

      play_mode = PLAYING_SYNTH;

      next_audio_sequence();
    }
  }

  void PicoSynth_I2S::next_audio_sequence() {
    // Clear any interrupt request caused by our channel
    //dma_channel_acknowledge_irq0(audio_dma_channel);
    // NOTE Temporary replacement of the above until this reaches pico-sdk main:
    // https://github.com/raspberrypi/pico-sdk/issues/974
    dma_hw->ints0 = 1u << audio_dma_channel;

    if(play_mode == PLAYING_SYNTH) {

      dma_channel_transfer_from_buffer_now(audio_dma_channel, tone_buffers[current_buffer], TONE_BUFFER_SIZE);
      current_buffer = (current_buffer + 1) % NUM_TONE_BUFFERS;

      populate_next_synth();
    }
    else {
      play_mode = NOT_PLAYING;
    }
  }

  void PicoSynth_I2S::populate_next_synth() {
    int16_t *samples = tone_buffers[current_buffer];
    for(uint i = 0; i < TONE_BUFFER_SIZE; i++) {
      samples[i] = synth.get_audio_frame();
    }
  }

  void PicoSynth_I2S::stop_playing() {
    if(picosynth == this) {
      // Stop the audio SM
      pio_sm_set_enabled(audio_pio, audio_sm, false);

      // Reset the I2S pins to avoid popping when audio is suddenly stopped
      const uint pins_to_clear = 1 << i2s_data | 1 << i2s_bclk | 1 << i2s_lrclk;
      pio_sm_set_pins_with_mask(audio_pio, audio_sm, 0, pins_to_clear);

      // Abort any in-progress DMA transfer
      dma_safe_abort(audio_dma_channel);

      play_mode = NOT_PLAYING;
    }
  }

  AudioChannel& PicoSynth_I2S::synth_channel(uint channel) {
    assert(channel < PicoSynth::CHANNEL_COUNT);
    return synth.channels[channel];
  }

  void PicoSynth_I2S::set_volume(float value) {
    value = value < 0.0f ? 0.0f : value;
    value = value > 1.0f ? 1.0f : value;
    this->volume = floor(value * 255.0f);
    this->synth.volume = this->volume * 255.0f;
  }

  float PicoSynth_I2S::get_volume() {
    return this->volume / 255.0f;
  }

  void PicoSynth_I2S::adjust_volume(float delta) {
    this->set_volume(this->get_volume() + delta);
  }

}
