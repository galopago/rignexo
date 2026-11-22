#include "mode_led_pio.h"

#include "board_config.h"
#include "mode_led.pio.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include <stdbool.h>
#include <stdint.h>

/* Prefer PIO1 SM0 — leave PIO0 free for other peripherals. */
#define MODE_LED_PIO pio1
#define MODE_LED_PIO_SM 0

static bool s_mode4;
static bool s_af_good;
static bool s_af_inited;
static uint32_t s_last_off_ms;
static uint32_t s_last_gap_ms;

static uint32_t ms_to_loops(uint32_t ms) {
  const uint32_t hz = clock_get_hz(clk_sys);
  return (hz / 1000u) * ms;
}

static void load_quiet_and_short(PIO pio, uint sm, uint32_t quiet_loops,
                                 uint32_t short_loops) {
  pio_sm_put(pio, sm, quiet_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_isr, pio_osr));

  pio_sm_put(pio, sm, short_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));
}

/* Mode 4: ISR=quiet, Y=OFF, OSR=GAP (left in OSR after final pull). */
static void load_mode4_delays(PIO pio, uint sm, uint32_t quiet_loops,
                              uint32_t off_loops, uint32_t gap_loops) {
  pio_sm_put(pio, sm, quiet_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_isr, pio_osr));

  pio_sm_put(pio, sm, off_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));

  pio_sm_put(pio, sm, gap_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  /* GAP remains in OSR for mov x, osr in the .pio program. */
}

static void apply_mode4_packet(bool packet_ok) {
  PIO pio = MODE_LED_PIO;
  const uint sm = MODE_LED_PIO_SM;
  const uint32_t quiet = ms_to_loops(BOARD_MODE_LED_QUIET_ON_MS);
  const uint32_t off_ms =
      packet_ok ? BOARD_MODE4_LED_OFF_PKT_MS : BOARD_MODE4_LED_OFF_BASELINE_MS;
  const uint32_t gap_ms =
      packet_ok ? BOARD_MODE4_LED_GAP_PKT_MS : BOARD_MODE4_LED_GAP_BASELINE_MS;
  const uint32_t off_loops = ms_to_loops(off_ms);
  const uint32_t gap_loops = ms_to_loops(gap_ms);

  s_last_off_ms = off_ms;
  s_last_gap_ms = gap_ms;

  pio_sm_set_enabled(pio, sm, false);
  load_mode4_delays(pio, sm, quiet, off_loops, gap_loops);
  pio_sm_set_enabled(pio, sm, true);
}

void mode_led_pio_start(device_mode_t mode) {
  PIO pio = MODE_LED_PIO;
  const uint sm = MODE_LED_PIO_SM;
  const uint pin = BOARD_MODE_LED_GPIO;

  /* Architect D15 / Option A: GPIO0 is MODE_LED only after this call. */
  const pio_program_t *program;
  pio_sm_config cfg;

  s_mode4 = (mode == DEVICE_MODE_APRS_TNC);
  s_af_good = false;
  s_af_inited = false;

  if (mode == DEVICE_MODE_APRS_TNC) {
    program = &mode_led_4pulse_program;
  } else if (mode == DEVICE_MODE_COMPOSITE_CAT) {
    program = &mode_led_3pulse_program;
  } else if (mode == DEVICE_MODE_AUDIO_VOX) {
    program = &mode_led_2pulse_program;
  } else {
    program = &mode_led_1pulse_program;
  }

  const uint offset = pio_add_program(pio, program);

  pio_gpio_init(pio, pin);
  gpio_set_function(pin, GPIO_FUNC_PIO1);
  pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

  if (mode == DEVICE_MODE_APRS_TNC) {
    cfg = mode_led_4pulse_program_get_default_config(offset);
  } else if (mode == DEVICE_MODE_COMPOSITE_CAT) {
    cfg = mode_led_3pulse_program_get_default_config(offset);
  } else if (mode == DEVICE_MODE_AUDIO_VOX) {
    cfg = mode_led_2pulse_program_get_default_config(offset);
  } else {
    cfg = mode_led_1pulse_program_get_default_config(offset);
  }
  sm_config_set_set_pins(&cfg, pin, 1);
  sm_config_set_clkdiv(&cfg, 1.0f);

  pio_sm_init(pio, sm, offset, &cfg);

  const uint32_t quiet = ms_to_loops(BOARD_MODE_LED_QUIET_ON_MS);

  if (s_mode4) {
    /* Idle → baseline 150/150 until FCS-OK. */
    const uint32_t off_ms = BOARD_MODE4_LED_OFF_BASELINE_MS;
    const uint32_t gap_ms = BOARD_MODE4_LED_GAP_BASELINE_MS;
    s_last_off_ms = off_ms;
    s_last_gap_ms = gap_ms;
    load_mode4_delays(pio, sm, quiet, ms_to_loops(off_ms), ms_to_loops(gap_ms));
    s_af_inited = true;
  } else {
    const uint32_t pulse = ms_to_loops(BOARD_MODE_LED_OFF_MS);
    (void)BOARD_MODE_LED_GAP_MS;
    s_last_off_ms = 0;
    s_last_gap_ms = 0;
    load_quiet_and_short(pio, sm, quiet, pulse);
  }

  if (BOARD_MODE_LED_ACTIVE_LEVEL) {
    pio_sm_set_pins_with_mask(pio, sm, 1u << pin, 1u << pin);
  } else {
    pio_sm_set_pins_with_mask(pio, sm, 0, 1u << pin);
  }

  pio_sm_set_enabled(pio, sm, true);
}

void mode_led_pio_set_af_good(bool packet_ok) {
  if (!s_mode4) {
    return;
  }
  if (s_af_inited && packet_ok == s_af_good) {
    return;
  }
  s_af_good = packet_ok;
  s_af_inited = true;
  apply_mode4_packet(packet_ok);
}

void mode_led_pio_af_dbg_get(uint32_t *off_ms, uint32_t *gap_ms,
                             uint32_t *tick_ms, bool *good) {
  if (off_ms != NULL) {
    *off_ms = s_mode4 ? s_last_off_ms : 0u;
  }
  if (gap_ms != NULL) {
    *gap_ms = s_mode4 ? s_last_gap_ms : 0u;
  }
  if (tick_ms != NULL) {
    *tick_ms = s_mode4 ? BOARD_MODE4_LED_TICK_MS : 0u;
  }
  if (good != NULL) {
    *good = s_mode4 && s_af_good;
  }
}
