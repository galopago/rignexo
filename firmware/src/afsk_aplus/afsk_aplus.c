/*
 * RIGNEXO clean-room A+ AFSK demod (D31 baseline; D33 revert of D32).
 * Algorithm intent from WB2OSZ "Building a Better Demodulator for APRS — Part 1"
 * (relative mark/space + geometric space-gain ladder). No Direwolf/GPL code.
 *
 * Path: DC remove → IQ envelopes → peak/valley normalize → soft = m_n - G*s_n
 * → N=5 slicers → DPLL (pll -= pll>>2) → HDLC → dup suppress.
 * No bandpass prefilter, no conf/eye skip gate, no hard DCD block of HDLC.
 */

#include "afsk_aplus.h"

#include "board_config.h"
#include "hdlc_ax25.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define APLUS_FS ((float)BOARD_AFSK_RX_SAMPLE_RATE_HZ)
#define APLUS_SPB ((int)(BOARD_AFSK_RX_SAMPLE_RATE_HZ / BOARD_AFSK_BAUD))
#define APLUS_DATA_MAX 514
#define APLUS_MIN_LEN 3

#define PLL_STEP ((int32_t)(((1ULL << 32) + (unsigned)APLUS_SPB / 2u) / (unsigned)APLUS_SPB))

typedef enum { SL_FLAG = 0, SL_DATA = 1 } sl_state_t;

typedef struct {
  float i;
  float q;
  float phase;
  float dphi;
  float decay;
  float peak;
  float valley;
} tone_iq_t;

typedef struct {
  float space_gain;
  int32_t pll;
  int prev_sym;
  int nrzi;
  sl_state_t state;
  uint8_t flag;
  uint8_t data_byte;
  unsigned data_bit_cnt;
  unsigned data_cnt;
  uint8_t data[APLUS_DATA_MAX];
  uint32_t flag_hunt;
  uint32_t entered_data;
  uint32_t bits;
  uint32_t ok;
  uint32_t bad_fcs;
  uint32_t abort;
} slicer_t;

static afsk_aplus_frame_cb_t s_cb;
static void *s_user;
static tone_iq_t s_mark;
static tone_iq_t s_space;
static slicer_t s_sl[AFSK_APLUS_N_SLICERS];
static float s_dc;
static float s_ene;
static int s_dcd;
static int32_t s_adc_min;
static int32_t s_adc_max;
static bool s_adc_init;
static uint32_t s_dup_drop;
static uint32_t s_agg_ok;
static uint32_t s_agg_bad;
static uint32_t s_agg_abort;
static uint32_t s_agg_bits;
static uint32_t s_agg_flag;
static uint32_t s_agg_entered;
static uint8_t s_last_slicer;
static uint8_t s_last_payload[APLUS_DATA_MAX];
static size_t s_last_len;
static uint16_t s_last_crc;
static bool s_have_last;

static void tone_init(tone_iq_t *t, float hz, float tau_s) {
  t->i = 0.f;
  t->q = 0.f;
  t->phase = 0.f;
  t->dphi = (float)(2.0 * M_PI) * hz / APLUS_FS;
  t->decay = expf(-1.f / (APLUS_FS * tau_s));
  t->peak = 1e-3f;
  t->valley = 1e-3f;
}

static float tone_amp_norm(tone_iq_t *t, float x) {
  float c;
  float s;
  float amp;
  float one_m;

  t->phase += t->dphi;
  if (t->phase > (float)(2.0 * M_PI)) {
    t->phase -= (float)(2.0 * M_PI);
  }
  c = cosf(t->phase);
  s = sinf(t->phase);
  one_m = 1.f - t->decay;
  t->i = t->i * t->decay + (x * c) * one_m;
  t->q = t->q * t->decay + (x * (-s)) * one_m;
  amp = sqrtf(t->i * t->i + t->q * t->q);

  if (amp > t->peak) {
    t->peak += (amp - t->peak) * 0.5f;
  } else {
    t->peak += (amp - t->peak) * 0.002f;
  }
  if (amp < t->valley) {
    t->valley += (amp - t->valley) * 0.5f;
  } else {
    t->valley += (amp - t->valley) * 0.002f;
  }
  {
    float span = t->peak - t->valley;
    if (span < 1e-6f) {
      span = 1e-6f;
    }
    return (amp - t->valley) / span;
  }
}

static void emit_frame(int si, const uint8_t *payload, size_t len) {
  uint16_t crc;

  if (payload == NULL || len == 0) {
    return;
  }
  crc = hdlc_crc16(payload, len);
  if (s_have_last && len == s_last_len && crc == s_last_crc &&
      memcmp(payload, s_last_payload, len) == 0) {
    s_dup_drop++;
    return;
  }
  if (len <= APLUS_DATA_MAX) {
    memcpy(s_last_payload, payload, len);
    s_last_len = len;
    s_last_crc = crc;
    s_have_last = true;
  }
  s_last_slicer = (uint8_t)si;
  if (s_cb) {
    s_cb(payload, len, s_user);
  }
}

static void slicer_output(slicer_t *sl, int si) {
  size_t len = sl->data_cnt;

  if (len < APLUS_MIN_LEN) {
    return;
  }
  {
    const size_t body = len - 2u;
    const uint16_t calc = hdlc_crc16(sl->data, body);
    const uint16_t got =
        (uint16_t)sl->data[body] | ((uint16_t)sl->data[body + 1u] << 8);
    if (calc != got) {
      sl->bad_fcs++;
      s_agg_bad++;
      return;
    }
    sl->ok++;
    s_agg_ok++;
    emit_frame(si, sl->data, body);
  }
}

static void slicer_bit(slicer_t *sl, int si, int bit) {
  sl->bits++;
  s_agg_bits++;
  sl->flag = (uint8_t)((sl->flag << 1) | (bit & 1));

  switch (sl->state) {
    case SL_FLAG:
      if (sl->flag == 0x7eu) {
        sl->flag_hunt++;
        s_agg_flag++;
        sl->state = SL_DATA;
        sl->entered_data++;
        s_agg_entered++;
        sl->data_cnt = 0;
        sl->data_bit_cnt = 0;
        sl->data_byte = 0;
      }
      break;

    case SL_DATA:
      if ((sl->flag & 0x3fu) == 0x3fu) {
        slicer_output(sl, si);
        sl->state = SL_FLAG;
        break;
      }
      if ((sl->flag & 0x3fu) == 0x3eu) {
        break;
      }
      sl->data_byte = (uint8_t)((sl->data_byte >> 1) | ((bit & 1) << 7));
      sl->data_bit_cnt++;
      if (sl->data_bit_cnt >= 8u) {
        if (sl->data_cnt < APLUS_DATA_MAX) {
          sl->data[sl->data_cnt++] = sl->data_byte;
        } else {
          sl->abort++;
          s_agg_abort++;
          sl->state = SL_FLAG;
        }
        sl->data_bit_cnt = 0;
      }
      break;

    default:
      sl->state = SL_FLAG;
      break;
  }
}

static void slicer_soft(slicer_t *sl, int si, float soft) {
  int sym = (soft >= 0.f) ? 1 : 0;
  int32_t prev_msb = sl->pll >> 31;

  sl->pll += PLL_STEP;
  if ((sl->pll >> 31) < prev_msb) {
    int bit = (sym == sl->nrzi) ? 1 : 0;
    sl->nrzi = sym;
    slicer_bit(sl, si, bit);
  }
  if (sym != sl->prev_sym) {
    sl->pll -= sl->pll >> 2;
    sl->prev_sym = sym;
  }
}

static void build_gains(void) {
  const float g0 = 0.5f;
  const float g1 = 4.0f;
  const int n = AFSK_APLUS_N_SLICERS;
  float ratio;
  int i;

  if (n <= 1) {
    s_sl[0].space_gain = 1.f;
    return;
  }
  ratio = powf(g1 / g0, 1.f / (float)(n - 1));
  s_sl[0].space_gain = g0;
  for (i = 1; i < n; i++) {
    s_sl[i].space_gain = s_sl[i - 1].space_gain * ratio;
  }
}

void afsk_aplus_init(afsk_aplus_frame_cb_t cb, void *user) {
  int i;

  s_cb = cb;
  s_user = user;
  memset(s_sl, 0, sizeof(s_sl));
  build_gains();
  tone_init(&s_mark, (float)BOARD_AFSK_MARK_HZ, 0.0007f);
  tone_init(&s_space, (float)BOARD_AFSK_SPACE_HZ, 0.0007f);
  s_dc = 0.f;
  s_ene = 0.f;
  s_dcd = 0;
  s_adc_init = false;
  s_dup_drop = 0;
  s_agg_ok = s_agg_bad = s_agg_abort = s_agg_bits = 0;
  s_agg_flag = s_agg_entered = 0;
  s_last_slicer = 0;
  s_have_last = false;
  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    s_sl[i].state = SL_FLAG;
    s_sl[i].nrzi = 1;
    s_sl[i].prev_sym = 1;
    s_sl[i].pll = 0;
  }
}

void afsk_aplus_reset(void) {
  int i;
  float gains[AFSK_APLUS_N_SLICERS];

  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    gains[i] = s_sl[i].space_gain;
  }
  memset(s_sl, 0, sizeof(s_sl));
  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    s_sl[i].space_gain = gains[i];
    s_sl[i].state = SL_FLAG;
    s_sl[i].nrzi = 1;
    s_sl[i].prev_sym = 1;
  }
  s_mark.i = s_mark.q = 0.f;
  s_space.i = s_space.q = 0.f;
  s_mark.peak = s_space.peak = 1e-3f;
  s_mark.valley = s_space.valley = 1e-3f;
  s_dc = 0.f;
  s_ene = 0.f;
  s_dcd = 0;
  s_have_last = false;
}

void afsk_aplus_process(float x) {
  float m;
  float s;
  float soft;
  int i;

  s_dc = s_dc * 0.999f + x * 0.001f;
  x -= s_dc;

  m = tone_amp_norm(&s_mark, x);
  s = tone_amp_norm(&s_space, x);

  {
    float e = m + s;
    s_ene = s_ene * 0.99f + e * 0.01f;
    if (!s_dcd && s_ene > BOARD_AFSK_APLUS_DCD_OPEN) {
      s_dcd = 1;
    } else if (s_dcd && s_ene < BOARD_AFSK_APLUS_DCD_CLOSE) {
      s_dcd = 0;
    }
  }

  /* D31: always run slicers — DCD is STAT/CSMA only, never blocks HDLC. */
  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    soft = m - s_sl[i].space_gain * s;
    slicer_soft(&s_sl[i], i, soft);
  }
}

void afsk_aplus_process_adc12(uint16_t adc12) {
  float x;
  int32_t v = (int32_t)adc12;

  if (!s_adc_init) {
    s_adc_min = v;
    s_adc_max = v;
    s_adc_init = true;
  } else {
    if (v < s_adc_min) {
      s_adc_min = v;
    }
    if (v > s_adc_max) {
      s_adc_max = v;
    }
  }
  x = ((float)v - 2048.f) / 2048.f;
  afsk_aplus_process(x);
}

bool afsk_aplus_dcd(void) {
  return s_dcd != 0;
}

void afsk_aplus_dbg_get(afsk_aplus_dbg_t *out) {
  int i;
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->flag_hunt = s_agg_flag;
  out->entered_data = s_agg_entered;
  out->bits = s_agg_bits;
  out->ok = s_agg_ok;
  out->bad_fcs = s_agg_bad;
  out->abort = s_agg_abort;
  out->dup_drop = s_dup_drop;
  out->adc_min = s_adc_init ? s_adc_min : 0;
  out->adc_max = s_adc_init ? s_adc_max : 0;
  out->m_amp = sqrtf(s_mark.i * s_mark.i + s_mark.q * s_mark.q);
  out->s_amp = sqrtf(s_space.i * s_space.i + s_space.q * s_space.q);
  out->dcd = (uint8_t)(s_dcd ? 1u : 0u);
  out->last_slicer = s_last_slicer;
  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    out->ok_per[i] = s_sl[i].ok;
    out->bad_per[i] = s_sl[i].bad_fcs;
    out->flag_per[i] = s_sl[i].flag_hunt;
  }
}

void afsk_aplus_dbg_clear(void) {
  int i;
  s_agg_ok = s_agg_bad = s_agg_abort = s_agg_bits = 0;
  s_agg_flag = s_agg_entered = 0;
  s_dup_drop = 0;
  s_adc_init = false;
  for (i = 0; i < AFSK_APLUS_N_SLICERS; i++) {
    s_sl[i].ok = 0;
    s_sl[i].bad_fcs = 0;
    s_sl[i].abort = 0;
    s_sl[i].bits = 0;
    s_sl[i].flag_hunt = 0;
    s_sl[i].entered_data = 0;
  }
}
