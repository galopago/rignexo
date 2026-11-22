/*
 * Port of amedes/pico_tnc RX demod + HDLC decode (BSD-3).
 * Copyright (c) 2021, Kazuhisa Yokota, JN1DFF — original decode/demod logic.
 * RIGNEXO glue / D30 field AGC / post-AGC CDT / B1 slice: 2026.
 *
 * Upstream: https://github.com/amedes/pico_tnc commit 0711f6b6673a0cf8b323b610253152d7a8fd4998
 */

#include "pico_tnc_rx.h"

#include "bell202_rx.h"
#include "board_config.h"
#include "filter.h"
#include "tnc_rx_types.h"

#include <string.h>

#define FCS_OK 0x0f47
#define MIN_LEN (7 * 2 + 1 + 1 + 2)
#define AX25_FLAG 0x7e
#define LPF_THRESHOLD (1 << 12) /* pico_tnc fixed slice — debug secondary only */

#define AVERAGE_MUL 256
#define AVERAGE_SHIFT 6
#define CDT_MUL 256
#define CDT_SHIFT 6

#define PLL_STEP ((int)(((1ULL << 32) + SAMPLING_N / 2) / SAMPLING_N))

#define CRC16_POLY 0x10811

static tnc_rx_t s_tnc;
static pico_tnc_frame_cb_t s_frame_cb;
static void *s_frame_user;
static int32_t s_adc_min;
static int32_t s_adc_max;
static bool s_adc_init;
static int32_t s_dem_min;
static int32_t s_dem_max;
static bool s_dem_init;

/* D30 AGC / B1 state */
static int32_t s_agc_peak;
static int32_t s_agc_valley;
static int32_t s_agc_gain_q8;
static int32_t s_agc_noise_n;
static int32_t s_xg_peak; /* post-AGC |x_g| peak (soft CDT) */
static int32_t s_dem_env;
static uint8_t s_adapt; /* always 1 for D30 primary path */

static int mix_env(int32_t env, int32_t x, int attack_shift, int decay_shift) {
  if (x > env) {
    return (int)(env + ((x - env) >> attack_shift));
  }
  return (int)(env + ((x - env) >> decay_shift));
}

/* Software CCITT CRC matching pico_tnc ax25_fcs residual check. */
static int ax25_fcs_sw(const uint8_t *packet, int length) {
  int i, j;
  unsigned crc;

  if (length <= 0) {
    return -1;
  }

  crc = 0xffff;
  for (i = 0; i < length; i++) {
    crc ^= packet[i];
    for (j = 0; j < 8; j++) {
      if (crc & 1u) {
        crc ^= CRC16_POLY;
      }
      crc >>= 1;
    }
  }
  crc ^= 0xffff;
  return (int)crc;
}

static void output_packet(tnc_rx_t *tp) {
  int len = (int)tp->data_cnt;
  uint8_t *data = tp->data;

  if (len < MIN_LEN) {
    return;
  }

  if (ax25_fcs_sw(data, len) != FCS_OK) {
    tp->dbg_bad_fcs++;
    return;
  }

  tp->dbg_ok++;
  if (s_frame_cb != NULL && len >= 2) {
    s_frame_cb(data, (size_t)(len - 2), s_frame_user);
  }
}

static void decode_bit(tnc_rx_t *tp, int bit) {
  tp->dbg_bits++;
  tp->flag <<= 1;
  tp->flag |= (uint8_t)(bit & 1);

  switch (tp->state) {
    case FLAG:
      if (tp->flag == AX25_FLAG) {
        tp->state = DATA;
        tp->data_cnt = 0;
        tp->data_bit_cnt = 0;
      }
      break;

    case DATA:
      if ((tp->flag & 0x3f) == 0x3f) {
        output_packet(tp);
        tp->state = FLAG;
        break;
      }

      if ((tp->flag & 0x3f) == 0x3e) {
        break; /* delete stuffed 0 */
      }

      tp->data_byte >>= 1;
      tp->data_byte |= (uint8_t)((bit & 1) << 7);
      tp->data_bit_cnt++;
      if (tp->data_bit_cnt >= 8) {
        if (tp->data_cnt < DATA_LEN) {
          tp->data[tp->data_cnt++] = tp->data_byte;
        } else {
          tp->dbg_abort++;
          tp->state = FLAG;
        }
        tp->data_bit_cnt = 0;
      }
      break;

    default:
      tp->state = FLAG;
      break;
  }
}

static void decode2(tnc_rx_t *tp, int val) {
  int32_t prev_pll = tp->pll_counter >> 31;

  tp->pll_counter += PLL_STEP;

  if ((tp->pll_counter >> 31) < prev_pll) {
    decode_bit(tp, val == tp->nrzi);
    tp->nrzi = (uint8_t)val;
  }

  if (val != tp->pval) {
    tp->dbg_disc_tr++;
    tp->pll_counter -= tp->pll_counter >> 2;
    tp->pval = (uint8_t)val;
  }
}

/*
 * D30 A: fast-attack / slow-decay |ac| envelope + multiplicative Q8 gain.
 * Never uses (err)>>N that can round a positive climb to zero.
 */
static int pico_tnc_agc_apply(int adc_u8, int *avg_out, int *xg_out) {
  tnc_rx_t *tp = &s_tnc;
  int ac;
  int abs_ac;
  int32_t span;
  int32_t err_q;
  int32_t step;
  int32_t xg;
  int32_t target = (int32_t)BOARD_AFSK_AGC_TARGET_SPAN;

  tp->avg += (adc_u8 * AVERAGE_MUL - tp->avg) >> AVERAGE_SHIFT;
  *avg_out = (tp->avg + AVERAGE_MUL / 2) / AVERAGE_MUL;
  ac = adc_u8 - *avg_out;
  abs_ac = (ac >= 0) ? ac : -ac;

  /* Peak: fast attack / slow decay. Valley: fast drop / slow rise. */
  s_agc_peak = mix_env(s_agc_peak, abs_ac, BOARD_AFSK_AGC_ATTACK_SHIFT,
                       BOARD_AFSK_AGC_DECAY_SHIFT);
  if (abs_ac < s_agc_valley) {
    s_agc_valley += (abs_ac - s_agc_valley) >> BOARD_AFSK_AGC_ATTACK_SHIFT;
  } else {
    s_agc_valley += (abs_ac - s_agc_valley) >> BOARD_AFSK_AGC_DECAY_SHIFT;
  }
  if (s_agc_valley < 0) {
    s_agc_valley = 0;
  }
  if (s_agc_peak < 1) {
    s_agc_peak = 1;
  }

  span = s_agc_peak - s_agc_valley;
  if (span < 1) {
    span = 1;
  }

  if (abs_ac < (int)BOARD_AFSK_AGC_NOISE_FLOOR_LSB) {
    if (s_agc_noise_n < (int32_t)BOARD_AFSK_AGC_HOLD_N) {
      s_agc_noise_n++;
    }
  } else {
    s_agc_noise_n = 0;
  }

  if (s_agc_noise_n < (int32_t)BOARD_AFSK_AGC_HOLD_N) {
    err_q = target - span;
    if (err_q > 0) {
      step = (s_agc_gain_q8 * err_q) /
             (target * (int32_t)BOARD_AFSK_AGC_GAIN_DIV);
      if (step < 1) {
        step = 1;
      }
      s_agc_gain_q8 += step;
      if (s_agc_gain_q8 > (int32_t)BOARD_AFSK_AGC_GAIN_MAX_Q8) {
        s_agc_gain_q8 = (int32_t)BOARD_AFSK_AGC_GAIN_MAX_Q8;
      }
    } else if (err_q < 0) {
      step = (s_agc_gain_q8 * (-err_q)) /
             (target * (int32_t)BOARD_AFSK_AGC_GAIN_DIV_SLOW);
      if (step < 1) {
        step = 1;
      }
      s_agc_gain_q8 -= step;
      if (s_agc_gain_q8 < (int32_t)BOARD_AFSK_AGC_GAIN_MIN_Q8) {
        s_agc_gain_q8 = (int32_t)BOARD_AFSK_AGC_GAIN_MIN_Q8;
      }
    }
  }

  xg = ((int32_t)ac * s_agc_gain_q8) >> 8;
  if (xg > 127) {
    xg = 127;
  } else if (xg < -127) {
    xg = -127;
  }
  *xg_out = (int)xg;
  return (int)xg;
}

/* B1 adaptive |dem| slice — primary field path (pico ±4096 secondary). */
static void pico_tnc_b1_slice(tnc_rx_t *tp, int dem) {
  int abs_dem = (dem >= 0) ? dem : -dem;
  int32_t thr;

  s_dem_env = mix_env(s_dem_env, abs_dem, BOARD_AFSK_DEM_ATTACK_SHIFT,
                      BOARD_AFSK_DEM_DECAY_SHIFT);
  if (s_dem_env < 1) {
    s_dem_env = 1;
  }

  thr = ((int32_t)BOARD_AFSK_DEM_K_NUM * s_dem_env) >> BOARD_AFSK_DEM_K_SHIFT;
  if (thr < 1) {
    thr = 1;
  }

  if (s_dem_env < (int32_t)BOARD_AFSK_DEM_ENV_FLOOR) {
    return; /* silence — hold last bit */
  }

  /* Primary B1: pico_tnc polarity dem high → SPACE(0), low → MARK(1). */
  if (dem > (int)thr) {
    tp->bit = 0;
  } else if (dem < -(int)thr) {
    tp->bit = 1;
  } else if (s_dem_env >= (int32_t)(LPF_THRESHOLD * 2)) {
    /* Secondary fixed ±4096 OR when envelope already large. */
    if (dem <= -LPF_THRESHOLD) {
      tp->bit = 1;
    } else if (dem >= LPF_THRESHOLD) {
      tp->bit = 0;
    }
  }
  /* else hold last bit */
}

void pico_tnc_rx_process_adc(int adc_u8) {
  tnc_rx_t *tp = &s_tnc;
  int avg;
  int xg;
  int abs_xg;
  int fed;
  int dem;
  bool enable;

  if (adc_u8 < 0) {
    adc_u8 = 0;
  } else if (adc_u8 > 255) {
    adc_u8 = 255;
  }

  if (!s_adc_init) {
    s_adc_min = adc_u8;
    s_adc_max = adc_u8;
    s_adc_init = true;
  } else {
    if (adc_u8 < s_adc_min) {
      s_adc_min = adc_u8;
    }
    if (adc_u8 > s_adc_max) {
      s_adc_max = adc_u8;
    }
  }

  pico_tnc_agc_apply(adc_u8, &avg, &xg);
  abs_xg = (xg >= 0) ? xg : -xg;
  s_xg_peak = mix_env(s_xg_peak, abs_xg, BOARD_AFSK_AGC_ATTACK_SHIFT,
                      BOARD_AFSK_AGC_DECAY_SHIFT);

  /* D30 B: CDT / energy on post-AGC x_g (not pre-AGC). */
  tp->cdt_lvl += (xg * xg * CDT_MUL - tp->cdt_lvl) >> CDT_SHIFT;

  if (!tp->cdt && tp->cdt_lvl > (int)BOARD_AFSK_CDT_THR_OPEN) {
    tp->cdt = 1;
  } else if (tp->cdt && tp->cdt_lvl < (int)BOARD_AFSK_CDT_THR_CLOSE) {
    tp->cdt = 0;
  }

  /* Soft enable while AGC climbs on quiet-but-real AF. */
  enable = (tp->cdt != 0) ||
           (s_xg_peak >= (int32_t)BOARD_AFSK_CDT_SOFT_XG_PEAK);
  if (!enable) {
    return; /* skip BPF/slice this sample; AGC already advanced */
  }

  fed = avg + xg;
  if (fed < 0) {
    fed = 0;
  } else if (fed > 255) {
    fed = 255;
  }

  dem = bell202_decode(tp, fed);

  if (!s_dem_init) {
    s_dem_min = dem;
    s_dem_max = dem;
    s_dem_init = true;
  } else {
    if (dem < s_dem_min) {
      s_dem_min = dem;
    }
    if (dem > s_dem_max) {
      s_dem_max = dem;
    }
  }

  pico_tnc_b1_slice(tp, dem);
  decode2(tp, tp->bit);
}

static void pico_tnc_fe_reset(void) {
  s_agc_peak = BOARD_AFSK_AGC_TARGET_SPAN / 2;
  s_agc_valley = 0;
  s_agc_gain_q8 = 256; /* start at 1.0×; must climb on quiet */
  s_agc_noise_n = 0;
  s_xg_peak = 0;
  s_dem_env = BOARD_AFSK_DEM_ENV_FLOOR;
  s_adapt = 1;
  s_adc_init = false;
  s_dem_init = false;
}

void pico_tnc_rx_reset(void) {
  tnc_rx_t *tp = &s_tnc;
  filter_t lpf = tp->lpf;
  filter_t bpf = tp->bpf;

  memset(tp, 0, sizeof(*tp));
  tp->lpf = lpf;
  tp->bpf = bpf;
  tp->state = FLAG;
  if (tp->lpf.x) {
    memset(tp->lpf.x, 0, (size_t)((tp->lpf.size + 3) & ~3) * sizeof(int16_t));
    tp->lpf.index = 0;
  }
  if (tp->bpf.x) {
    memset(tp->bpf.x, 0, (size_t)((tp->bpf.size + 3) & ~3) * sizeof(int16_t));
    tp->bpf.index = 0;
  }
  pico_tnc_fe_reset();
}

void pico_tnc_rx_init(pico_tnc_frame_cb_t cb, void *user) {
  static const filter_param_t flt_lpf = {
      .size = FIR_LPF_N,
      .sampling_freq = SAMPLING_RATE,
      .pass_freq = 0,
      .cutoff_freq = 1200,
  };
  static const filter_param_t flt_bpf = {
      .size = FIR_BPF_N,
      .sampling_freq = SAMPLING_RATE,
      .pass_freq = 900,
      .cutoff_freq = 2500,
  };
  static int16_t *s_lpf_an;
  static int16_t *s_bpf_an;

  s_frame_cb = cb;
  s_frame_user = user;

  if (s_lpf_an == NULL) {
    s_lpf_an = filter_coeff(&flt_lpf);
  }
  if (s_bpf_an == NULL) {
    s_bpf_an = filter_coeff(&flt_bpf);
  }

  memset(&s_tnc, 0, sizeof(s_tnc));
  s_tnc.state = FLAG;
  if (s_lpf_an) {
    filter_init(&s_tnc.lpf, s_lpf_an, FIR_LPF_N);
  }
  if (s_bpf_an) {
    filter_init(&s_tnc.bpf, s_bpf_an, FIR_BPF_N);
  }
  pico_tnc_fe_reset();
}

bool pico_tnc_rx_cdt(void) {
  return s_tnc.cdt != 0;
}

void pico_tnc_rx_dbg_get(pico_tnc_rx_dbg_t *out) {
  if (!out) {
    return;
  }
  out->bits = s_tnc.dbg_bits;
  out->ok = s_tnc.dbg_ok;
  out->bad_fcs = s_tnc.dbg_bad_fcs;
  out->abort = s_tnc.dbg_abort;
  out->disc_tr = s_tnc.dbg_disc_tr;
  out->ene = s_tnc.cdt_lvl;
  out->adc_min = s_adc_init ? s_adc_min : 0;
  out->adc_max = s_adc_init ? s_adc_max : 0;
  out->dem_min = s_dem_init ? s_dem_min : 0;
  out->dem_max = s_dem_init ? s_dem_max : 0;
  out->agc_gain_q8 = s_agc_gain_q8;
  out->agc_peak = s_agc_peak;
  out->agc_valley = s_agc_valley;
  out->dem_env = s_dem_env;
  out->cdt = (uint8_t)(s_tnc.cdt ? 1u : 0u);
  out->adapt = s_adapt;
}

void pico_tnc_rx_dbg_clear(void) {
  s_tnc.dbg_bits = 0;
  s_tnc.dbg_ok = 0;
  s_tnc.dbg_bad_fcs = 0;
  s_tnc.dbg_abort = 0;
  s_tnc.dbg_disc_tr = 0;
  s_adc_init = false;
  s_dem_init = false;
  /* Keep live AGC/B1 envelopes — CLR resets tallies only. */
}
