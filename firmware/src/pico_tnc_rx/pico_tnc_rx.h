/**
 * RIGNEXO wrapper around amedes/pico_tnc RX modem (BSD-3).
 * Upstream: https://github.com/amedes/pico_tnc @ 0711f6b
 * D30: field AGC / post-AGC CDT / B1 adaptive slice (RIGNEXO clean-room).
 */

#ifndef PICO_TNC_RX_H
#define PICO_TNC_RX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pico_tnc_frame_cb_t)(const uint8_t *payload, size_t len, void *user);

void pico_tnc_rx_init(pico_tnc_frame_cb_t cb, void *user);
void pico_tnc_rx_reset(void);
/* A1: pass 8-bit sample 0…255 (from 12-bit adc_read() >> 4). */
void pico_tnc_rx_process_adc(int adc_sample_u8);

bool pico_tnc_rx_cdt(void);

/* Debug counters since last clear (wrap freely). */
typedef struct {
  uint32_t bits;
  uint32_t ok;
  uint32_t bad_fcs;
  uint32_t abort;
  uint32_t disc_tr;
  int32_t ene; /* post-AGC cdt_lvl for STAT */
  int32_t adc_min;
  int32_t adc_max;
  int32_t dem_min; /* sticky soft-decision extrema since CLR */
  int32_t dem_max;
  int32_t agc_gain_q8; /* 256 = 1.0 */
  int32_t agc_peak;    /* |ac| peak envelope */
  int32_t agc_valley;  /* |ac| valley envelope */
  int32_t dem_env;     /* |dem| envelope for B1 */
  uint8_t cdt;         /* 0/1 carrier detect */
  uint8_t adapt;       /* 1 = B1 adaptive slice primary */
} pico_tnc_rx_dbg_t;

void pico_tnc_rx_dbg_get(pico_tnc_rx_dbg_t *out);
void pico_tnc_rx_dbg_clear(void);

#ifdef __cplusplus
}
#endif

#endif
