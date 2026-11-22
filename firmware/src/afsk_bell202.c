#include "afsk_bell202.h"

#include "afsk_aplus.h"
#include "board_config.h"
#include "hdlc_ax25.h"
#include "kiss_cdc.h"
#include "mode_led_pio.h"
#include "ptt.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <stdio.h>
#include <string.h>

#define AFSK_FRAME_MAX       512U
#define AFSK_BITBUF_MAX      8192U
#define AFSK_SAMPLES_PER_BIT (BOARD_AFSK_SAMPLE_RATE_HZ / BOARD_AFSK_BAUD)
#define AFSK_RX_Q_SLOTS      2U

/* TX Bresenham @ 9600 Hz */
#define AFSK_TX_BASE_US ((1000000u / BOARD_AFSK_SAMPLE_RATE_HZ))
#define AFSK_TX_REM_INC ((1000000u % BOARD_AFSK_SAMPLE_RATE_HZ))

/* RX Bresenham @ 24000 Hz (D31 A+) */
#define AFSK_RX_BASE_US ((1000000u / BOARD_AFSK_RX_SAMPLE_RATE_HZ))
#define AFSK_RX_REM_INC ((1000000u % BOARD_AFSK_RX_SAMPLE_RATE_HZ))

static const int16_t s_sine[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
};
typedef enum {
  AFSK_IDLE = 0,
  AFSK_WAIT_CSMA,
  AFSK_TX,
} afsk_state_t;

static uint s_pwm_slice;
static uint s_pwm_channel;

static alarm_id_t s_tx_alarm;
static bool s_tx_alarm_on;
static int32_t s_tx_rem;

static alarm_id_t s_rx_alarm;
static bool s_rx_alarm_on;
static int32_t s_rx_rem;

static afsk_state_t s_state;
static uint32_t s_phase;
static uint32_t s_phase_inc;
static bool s_nrzi_mark;

static uint8_t s_bits[AFSK_BITBUF_MAX];
static size_t s_nbits;
static size_t s_bit_i;
static unsigned s_samp_in_bit;

static uint8_t s_pending[AFSK_FRAME_MAX];
static size_t s_pending_len;
static bool s_pending_valid;

static absolute_time_t s_csma_wait_until;
static uint32_t s_rng;

static uint32_t s_dbg_t_rx_a; /* shorter period ticks */
static uint32_t s_dbg_t_rx_b;
static volatile bool s_dbg_dump_req;
static bool s_dbg_dump_fail_sticky;

static uint8_t s_rx_q[AFSK_RX_Q_SLOTS][AFSK_FRAME_MAX];
static size_t s_rx_q_len[AFSK_RX_Q_SLOTS];
static volatile uint8_t s_rx_q_w;
static volatile uint8_t s_rx_q_r;
static volatile uint8_t s_rx_q_n;

/* D39/D40: MODE_LED packet blink (FCS-only, long OFF) + D38 STAT debug. */
typedef enum {
  AF_LED_REASON_NONE = 0,
  AF_LED_REASON_FCS,
  AF_LED_REASON_HOLD_EXP,
  AF_LED_REASON_CLR,
} af_led_reason_t;

static volatile uint16_t s_af_adc_min;
static volatile uint16_t s_af_adc_max;
static volatile bool s_af_adc_init;
static absolute_time_t s_af_led_next;
static absolute_time_t s_af_hold_until;
static bool s_af_hold_armed;
static af_led_reason_t s_af_reason;
static uint16_t s_af_last_amin;
static uint16_t s_af_last_amax;
static uint32_t s_af_last_pp;
static bool s_af_last_active;
static bool s_af_last_clip;

static const char *af_led_reason_str(af_led_reason_t r) {
  switch (r) {
  case AF_LED_REASON_FCS:
    return "fcs";
  case AF_LED_REASON_HOLD_EXP:
    return "hold_exp";
  case AF_LED_REASON_CLR:
    return "clr";
  default:
    return "none";
  }
}

static void afsk_af_led_arm_packet(af_led_reason_t reason) {
  s_af_hold_armed = true;
  s_af_hold_until = make_timeout_time_ms(BOARD_MODE4_AF_HOLD_MS);
  s_af_reason = reason;
  mode_led_pio_set_af_good(true);
}

static void afsk_af_led_force_baseline(af_led_reason_t reason) {
  s_af_hold_armed = false;
  s_af_reason = reason;
  mode_led_pio_set_af_good(false);
}

static uint32_t afsk_af_hold_remaining_ms(void) {
  if (!s_af_hold_armed) {
    return 0u;
  }
  const int64_t us =
      absolute_time_diff_us(get_absolute_time(), s_af_hold_until);
  if (us <= 0) {
    return 0u;
  }
  return (uint32_t)(us / 1000);
}

static uint32_t phase_inc_for_hz(uint32_t hz) {
  return (uint32_t)(((uint64_t)hz << 32) / (uint64_t)BOARD_AFSK_SAMPLE_RATE_HZ);
}

static uint16_t pcm_to_pwm(int16_t sample) {
  const int32_t centered = ((int32_t)sample + 32768) * (int32_t)BOARD_PWM_WRAP;
  return (uint16_t)(centered / 65536);
}

static void afsk_set_tone_mark(bool mark) {
  const uint32_t hz = mark ? BOARD_AFSK_MARK_HZ : BOARD_AFSK_SPACE_HZ;
  s_phase_inc = phase_inc_for_hz(hz);
}

static void afsk_pwm_idle(void) {
  pwm_set_chan_level(s_pwm_slice, s_pwm_channel, BOARD_PWM_WRAP / 2U);
}

static unsigned flags_for_tenths(uint8_t tenths) {
  const unsigned bits = (unsigned)tenths * 12u;
  unsigned flags = (bits + 7u) / 8u;
  if (flags < 1u) {
    flags = 1u;
  }
  return flags;
}

static void afsk_apply_bit_nrzi(unsigned bit) {
  if (bit == 0u) {
    s_nrzi_mark = !s_nrzi_mark;
  }
  afsk_set_tone_mark(s_nrzi_mark);
}

static uint8_t afsk_rand8(void) {
  s_rng = s_rng * 1664525u + 1013904223u;
  return (uint8_t)(s_rng >> 24);
}

static bool afsk_build_from_payload(const uint8_t *payload, size_t len) {
  const unsigned open_flags = flags_for_tenths(kiss_cdc_txdelay());
  const unsigned close_flags = flags_for_tenths(kiss_cdc_txtail());

  s_nbits = hdlc_tx_build_bits(payload, len, open_flags, close_flags, s_bits,
                               AFSK_BITBUF_MAX);
  if (s_nbits == 0) {
    return false;
  }

  s_bit_i = 0;
  s_samp_in_bit = 0;
  s_nrzi_mark = true;
  s_phase = 0;
  afsk_apply_bit_nrzi(s_bits[0]);
  return true;
}

static void afsk_rx_queue_frame(const uint8_t *payload, size_t len) {
  if (payload == NULL || len == 0 || len > AFSK_FRAME_MAX) {
    kiss_cdc_dbg_note_drop();
    return;
  }
  if (s_rx_q_n >= AFSK_RX_Q_SLOTS) {
    kiss_cdc_dbg_note_drop();
    return;
  }
  const uint8_t slot = s_rx_q_w;
  memcpy(s_rx_q[slot], payload, len);
  s_rx_q_len[slot] = len;
  s_rx_q_w = (uint8_t)((slot + 1u) % AFSK_RX_Q_SLOTS);
  s_rx_q_n++;
}

static void afsk_on_rx_frame(const uint8_t *payload, size_t len, void *user) {
  (void)user;
  afsk_rx_queue_frame(payload, len);
  /* D40.2: FCS-OK → long OFF (250) + 3 s hold. */
  afsk_af_led_arm_packet(AF_LED_REASON_FCS);
}

static void afsk_begin_tx(void) {
  s_state = AFSK_TX;
  afsk_aplus_reset();
  ptt_set_active(true);
}

static bool afsk_try_take_pending_into_tx(void);

static void afsk_finish_tx(void) {
  s_nbits = 0;
  s_bit_i = 0;
  afsk_pwm_idle();
  ptt_set_active(false);
  afsk_aplus_reset();

  if (s_tx_alarm_on && s_tx_alarm > 0) {
    cancel_alarm(s_tx_alarm);
    s_tx_alarm = 0;
    s_tx_alarm_on = false;
  }

  if (s_pending_valid) {
    s_state = AFSK_WAIT_CSMA;
    s_csma_wait_until = get_absolute_time();
  } else {
    s_state = AFSK_IDLE;
  }
}

static void afsk_tx_sample(void) {
  if (s_bit_i >= s_nbits) {
    afsk_finish_tx();
    return;
  }

  s_phase += s_phase_inc;
  const uint8_t idx = (uint8_t)(s_phase >> 24);
  pwm_set_chan_level(s_pwm_slice, s_pwm_channel, pcm_to_pwm(s_sine[idx]));

  s_samp_in_bit++;
  if (s_samp_in_bit >= AFSK_SAMPLES_PER_BIT) {
    s_samp_in_bit = 0;
    s_bit_i++;
    if (s_bit_i >= s_nbits) {
      afsk_finish_tx();
      return;
    }
    afsk_apply_bit_nrzi(s_bits[s_bit_i]);
  }
}

static int64_t afsk_tx_alarm_cb(alarm_id_t id, void *user_data) {
  (void)id;
  (void)user_data;

  if (s_state != AFSK_TX) {
    s_tx_alarm_on = false;
    return 0;
  }

  afsk_tx_sample();
  if (s_state != AFSK_TX) {
    s_tx_alarm_on = false;
    return 0;
  }

  s_tx_rem += (int32_t)AFSK_TX_REM_INC;
  int64_t delay = -((int64_t)AFSK_TX_BASE_US);
  if (s_tx_rem >= (int32_t)BOARD_AFSK_SAMPLE_RATE_HZ) {
    s_tx_rem -= (int32_t)BOARD_AFSK_SAMPLE_RATE_HZ;
    delay -= 1;
  }
  return delay;
}

static void afsk_start_tx_alarm(void) {
  if (s_tx_alarm_on) {
    return;
  }
  s_tx_rem = 0;
  s_tx_alarm = add_alarm_in_us((int64_t)AFSK_TX_BASE_US, afsk_tx_alarm_cb, NULL, true);
  s_tx_alarm_on = (s_tx_alarm > 0);
}

static int64_t afsk_rx_alarm_cb(alarm_id_t id, void *user_data) {
  (void)id;
  (void)user_data;

  if (s_state != AFSK_TX) {
    /* D31: 12-bit ADC → A+ float path @ 24 kHz. */
    const uint16_t raw12 = adc_read();
    if (!s_af_adc_init) {
      s_af_adc_min = raw12;
      s_af_adc_max = raw12;
      s_af_adc_init = true;
    } else {
      if (raw12 < s_af_adc_min) {
        s_af_adc_min = raw12;
      }
      if (raw12 > s_af_adc_max) {
        s_af_adc_max = raw12;
      }
    }
    afsk_aplus_process_adc12(raw12);
  }

  s_rx_rem += (int32_t)AFSK_RX_REM_INC;
  int64_t delay = -((int64_t)AFSK_RX_BASE_US);
  if (s_rx_rem >= (int32_t)BOARD_AFSK_RX_SAMPLE_RATE_HZ) {
    s_rx_rem -= (int32_t)BOARD_AFSK_RX_SAMPLE_RATE_HZ;
    delay -= 1;
    s_dbg_t_rx_b++;
  } else {
    s_dbg_t_rx_a++;
  }
  return delay;
}

static void afsk_ensure_rx_timer(void) {
  if (s_rx_alarm_on) {
    return;
  }
  s_rx_rem = 0;
  s_rx_alarm = add_alarm_in_us((int64_t)AFSK_RX_BASE_US, afsk_rx_alarm_cb, NULL, true);
  s_rx_alarm_on = (s_rx_alarm > 0);
}

static void afsk_csma_enter(void) {
  s_state = AFSK_WAIT_CSMA;
  s_csma_wait_until = get_absolute_time();
}

static bool afsk_try_take_pending_into_tx(void) {
  if (!s_pending_valid) {
    return false;
  }
  if (!afsk_build_from_payload(s_pending, s_pending_len)) {
    s_pending_valid = false;
    s_pending_len = 0;
    return false;
  }
  s_pending_valid = false;
  s_pending_len = 0;
  afsk_begin_tx();
  afsk_start_tx_alarm();
  return true;
}

void afsk_init(void) {
  gpio_set_function(BOARD_RADIO_AUDIO_OUT_GPIO, GPIO_FUNC_PWM);
  s_pwm_slice = pwm_gpio_to_slice_num(BOARD_RADIO_AUDIO_OUT_GPIO);
  s_pwm_channel = pwm_gpio_to_channel(BOARD_RADIO_AUDIO_OUT_GPIO);

  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv(&cfg, BOARD_PWM_CLKDIV);
  pwm_config_set_wrap(&cfg, BOARD_PWM_WRAP);
  pwm_init(s_pwm_slice, &cfg, true);
  afsk_pwm_idle();

  adc_init();
  adc_gpio_init(BOARD_RADIO_AUDIO_IN_GPIO);
  adc_select_input(BOARD_ADC_INPUT_INDEX);

  s_state = AFSK_IDLE;
  s_tx_alarm_on = false;
  s_rx_alarm_on = false;
  s_tx_alarm = 0;
  s_rx_alarm = 0;
  s_pending_valid = false;
  s_pending_len = 0;
  s_nbits = 0;
  s_bit_i = 0;
  s_samp_in_bit = 0;
  s_nrzi_mark = true;
  s_phase = 0;
  s_phase_inc = phase_inc_for_hz(BOARD_AFSK_MARK_HZ);
  s_rng = to_ms_since_boot(get_absolute_time()) ^ 0xA5A5A5A5u;
  s_rx_q_w = 0;
  s_rx_q_r = 0;
  s_rx_q_n = 0;
  s_dbg_dump_req = false;
  s_dbg_dump_fail_sticky = false;
  s_dbg_t_rx_a = 0;
  s_dbg_t_rx_b = 0;
  s_af_adc_init = false;
  s_af_last_amin = 0;
  s_af_last_amax = 0;
  s_af_last_pp = 0;
  s_af_last_active = false;
  s_af_last_clip = false;
  s_af_led_next = make_timeout_time_ms(BOARD_MODE4_AF_UPDATE_MS);
  /* D39: mode entry → baseline 150/150. */
  afsk_af_led_force_baseline(AF_LED_REASON_NONE);

  afsk_aplus_init(afsk_on_rx_frame, NULL);
  afsk_ensure_rx_timer();
}

void afsk_rx_debug_clear(void) {
  s_dbg_t_rx_a = 0;
  s_dbg_t_rx_b = 0;
  s_dbg_dump_fail_sticky = false;
  afsk_aplus_dbg_clear();
  kiss_cdc_dbg_clear();
  /* D39: CLR clears packet hold → baseline 150/150. */
  afsk_af_led_force_baseline(AF_LED_REASON_CLR);
}

void afsk_rx_debug_request_dump(void) {
  s_dbg_dump_req = true;
}

static bool afsk_cdc_write_str(const char *s) {
  if (s == NULL || !tud_cdc_connected()) {
    return false;
  }
  const size_t n = strlen(s);
  if (n == 0) {
    return true;
  }

  size_t off = 0;
  const absolute_time_t deadline = make_timeout_time_ms(100);

  while (off < n) {
    if (!tud_cdc_connected()) {
      return false;
    }
    const uint32_t avail = tud_cdc_write_available();
    if (avail == 0u) {
      tud_cdc_write_flush();
      tud_task();
      if (time_reached(deadline)) {
        return false;
      }
      busy_wait_us(50);
      continue;
    }
    size_t chunk = n - off;
    if (chunk > (size_t)avail) {
      chunk = (size_t)avail;
    }
    const uint32_t wrote = tud_cdc_write(s + off, chunk);
    if (wrote == 0u) {
      tud_cdc_write_flush();
      tud_task();
      if (time_reached(deadline)) {
        return false;
      }
      busy_wait_us(50);
      continue;
    }
    off += (size_t)wrote;
  }
  tud_cdc_write_flush();
  tud_task();
  return true;
}

static void afsk_rx_debug_dump_now(void) {
  char line[192];
  afsk_aplus_dbg_t d;
  bool complete = true;
  const uint32_t prev_fail = s_dbg_dump_fail_sticky ? 1u : 0u;
  uint32_t led_off = 0;
  uint32_t led_gap = 0;
  uint32_t led_tick = 0;
  bool led_good = false;

  afsk_aplus_dbg_get(&d);
  mode_led_pio_af_dbg_get(&led_off, &led_gap, &led_tick, &led_good);

  const int32_t amid = (d.adc_min + d.adc_max) / 2;
  const uint32_t dcd = d.dcd ? 1u : 0u;

  snprintf(line, sizeof(line),
           "RGNXDBG L0 dcd=%lu adc_min=%ld adc_max=%ld adc_mid=%ld "
           "m=%.3f s=%.3f\n",
           (unsigned long)dcd, (long)d.adc_min, (long)d.adc_max, (long)amid,
           (double)d.m_amp, (double)d.s_amp);
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG L1 flag=%lu entered=%lu dup_drop=%lu last_sl=%u\n",
           (unsigned long)d.flag_hunt, (unsigned long)d.entered_data,
           (unsigned long)d.dup_drop, (unsigned)d.last_slicer);
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG L2 bits=%lu abort=%lu bad_fcs=%lu ok=%lu\n",
           (unsigned long)d.bits, (unsigned long)d.abort,
           (unsigned long)d.bad_fcs, (unsigned long)d.ok);
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG L3 kiss_ok=%lu kiss_drop=%lu ok0=%lu ok1=%lu ok2=%lu "
           "ok3=%lu ok4=%lu\n",
           (unsigned long)kiss_cdc_dbg_kiss_ok(),
           (unsigned long)kiss_cdc_dbg_kiss_drop(),
           (unsigned long)d.ok_per[0], (unsigned long)d.ok_per[1],
           (unsigned long)d.ok_per[2], (unsigned long)d.ok_per[3],
           (unsigned long)d.ok_per[4]);
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG Lclk spb_x100=2000 t_a=%lu t_b=%lu tx_busy=%lu "
           "dump_fail=%lu n_sl=%u\n",
           (unsigned long)s_dbg_t_rx_a, (unsigned long)s_dbg_t_rx_b,
           (unsigned long)(afsk_tx_busy() ? 1u : 0u),
           (unsigned long)prev_fail, (unsigned)AFSK_APLUS_N_SLICERS);
  complete = afsk_cdc_write_str(line) && complete;

  /* D38: AF / packet-quality LED metric + PIO OFF timer. */
  snprintf(line, sizeof(line),
           "RGNXDBG Laf af_pp=%lu af_amin=%u af_amax=%u af_active=%u "
           "af_clip=%u af_good=%u af_hold=%u af_hold_ms=%lu af_reason=%s\n",
           (unsigned long)s_af_last_pp, (unsigned)s_af_last_amin,
           (unsigned)s_af_last_amax, s_af_last_active ? 1u : 0u,
           s_af_last_clip ? 1u : 0u, s_af_hold_armed ? 1u : 0u,
           s_af_hold_armed ? 1u : 0u, (unsigned long)afsk_af_hold_remaining_ms(),
           af_led_reason_str(s_af_reason));
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG Lafcfg pp_act=%u pp_bad=%u pp_good=%u hold_cfg_ms=%lu\n",
           (unsigned)BOARD_MODE4_AF_PP_ACTIVITY, (unsigned)BOARD_MODE4_AF_PP_BAD,
           (unsigned)BOARD_MODE4_AF_PP_GOOD,
           (unsigned long)BOARD_MODE4_AF_HOLD_MS);
  complete = afsk_cdc_write_str(line) && complete;

  snprintf(line, sizeof(line),
           "RGNXDBG Lled led_off_ms=%lu led_gap_ms=%lu led_tick_ms=%lu "
           "led_good=%u\n",
           (unsigned long)led_off, (unsigned long)led_gap,
           (unsigned long)led_tick, led_good ? 1u : 0u);
  complete = afsk_cdc_write_str(line) && complete;

  complete = afsk_cdc_write_str("RGNXDBG END\n") && complete;
  s_dbg_dump_fail_sticky = !complete;
}

void afsk_shutdown(void) {
  if (s_rx_alarm_on && s_rx_alarm > 0) {
    cancel_alarm(s_rx_alarm);
  }
  if (s_tx_alarm_on && s_tx_alarm > 0) {
    cancel_alarm(s_tx_alarm);
  }
  s_rx_alarm_on = false;
  s_tx_alarm_on = false;
  s_rx_alarm = 0;
  s_tx_alarm = 0;
  s_state = AFSK_IDLE;
  s_pending_valid = false;
  s_nbits = 0;
  s_rx_q_n = 0;
  afsk_pwm_idle();
  ptt_force_inactive();
}

bool afsk_tx_busy(void) {
  return s_state == AFSK_TX;
}

bool afsk_dcd(void) {
  return afsk_aplus_dcd();
}

bool afsk_tx_queue_frame(const uint8_t *payload, size_t len) {
  if (payload == NULL || len == 0 || len > AFSK_FRAME_MAX) {
    return false;
  }

  if (s_state == AFSK_IDLE) {
    memcpy(s_pending, payload, len);
    s_pending_len = len;
    s_pending_valid = true;
    afsk_csma_enter();
    return true;
  }

  if (s_state == AFSK_WAIT_CSMA) {
    if (s_pending_valid) {
      return false;
    }
    memcpy(s_pending, payload, len);
    s_pending_len = len;
    s_pending_valid = true;
    return true;
  }

  if (s_pending_valid) {
    return false;
  }
  memcpy(s_pending, payload, len);
  s_pending_len = len;
  s_pending_valid = true;
  return true;
}

static void afsk_af_led_update(void) {
  uint16_t amin;
  uint16_t amax;
  uint32_t pp;
  bool active;
  bool clip;
  uint32_t ints;

  /* D39: hold expiry is CPU-owned; amplitude never arms packet blink. */
  if (s_af_hold_armed && time_reached(s_af_hold_until)) {
    afsk_af_led_force_baseline(AF_LED_REASON_HOLD_EXP);
  }

  if (!time_reached(s_af_led_next)) {
    return;
  }
  s_af_led_next = make_timeout_time_ms(BOARD_MODE4_AF_UPDATE_MS);

  ints = save_and_disable_interrupts();
  if (!s_af_adc_init || s_state == AFSK_TX) {
    amin = 0;
    amax = 0;
    s_af_adc_init = false;
  } else {
    amin = s_af_adc_min;
    amax = s_af_adc_max;
    s_af_adc_init = false;
  }
  restore_interrupts(ints);

  pp = (amax >= amin) ? (uint32_t)(amax - amin) : 0u;
  clip = (amin <= 16u && amax >= 4080u);
  active = (pp >= (uint32_t)BOARD_MODE4_AF_PP_ACTIVITY);

  /* D38: sticky last window for STAT Laf only (no promote path). */
  s_af_last_amin = amin;
  s_af_last_amax = amax;
  s_af_last_pp = pp;
  s_af_last_active = active;
  s_af_last_clip = clip;
}

void afsk_task(void) {
  if (s_dbg_dump_req) {
    s_dbg_dump_req = false;
    afsk_rx_debug_dump_now();
  }

  afsk_af_led_update();

  for (;;) {
    uint8_t local[AFSK_FRAME_MAX];
    size_t len = 0;
    uint32_t ints = save_and_disable_interrupts();
    if (s_rx_q_n == 0) {
      restore_interrupts(ints);
      break;
    }
    const uint8_t slot = s_rx_q_r;
    len = s_rx_q_len[slot];
    memcpy(local, s_rx_q[slot], len);
    s_rx_q_r = (uint8_t)((slot + 1u) % AFSK_RX_Q_SLOTS);
    s_rx_q_n--;
    restore_interrupts(ints);
    kiss_cdc_send_frame(local, len);
  }

  if (s_state != AFSK_WAIT_CSMA || !s_pending_valid) {
    return;
  }

  if (!time_reached(s_csma_wait_until)) {
    return;
  }

  const bool fullduplex = kiss_cdc_fullduplex() != 0;

  if (!fullduplex && afsk_dcd()) {
    const uint32_t slot_ms = (uint32_t)kiss_cdc_slottime() * 10u;
    s_csma_wait_until = make_timeout_time_ms(slot_ms > 0 ? slot_ms : 10u);
    return;
  }

  if (!fullduplex) {
    const uint8_t r = afsk_rand8();
    if (r > kiss_cdc_persist()) {
      const uint32_t slot_ms = (uint32_t)kiss_cdc_slottime() * 10u;
      s_csma_wait_until = make_timeout_time_ms(slot_ms > 0 ? slot_ms : 10u);
      return;
    }
  }

  (void)afsk_try_take_pending_into_tx();
}
