/**
 * Clean-room Direwolf-class A+ multi-slicer AFSK RX (D31 baseline).
 * Ideas: WB2OSZ Better APRS Demodulator Part 1 — no GPL source.
 * D33: D32 WEAK fully reverted — this is the live baseline again.
 */

#ifndef AFSK_APLUS_H
#define AFSK_APLUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AFSK_APLUS_N_SLICERS
#define AFSK_APLUS_N_SLICERS 5
#endif

typedef void (*afsk_aplus_frame_cb_t)(const uint8_t *payload, size_t len,
                                     void *user);

void afsk_aplus_init(afsk_aplus_frame_cb_t cb, void *user);
void afsk_aplus_reset(void);

/* One sample at BOARD_AFSK_RX_SAMPLE_RATE_HZ, float ~[-1,1] after DC remove. */
void afsk_aplus_process(float x);

/* Convenience: 12-bit ADC mid-scale ~2048 → float. */
void afsk_aplus_process_adc12(uint16_t adc12);

bool afsk_aplus_dcd(void);

typedef struct {
  uint32_t flag_hunt;     /* aggregate flag sightings */
  uint32_t entered_data;  /* slicers that entered DATA */
  uint32_t bits;
  uint32_t ok;
  uint32_t bad_fcs;
  uint32_t abort;
  uint32_t dup_drop;
  uint32_t ok_per[AFSK_APLUS_N_SLICERS];
  uint32_t bad_per[AFSK_APLUS_N_SLICERS];
  uint32_t flag_per[AFSK_APLUS_N_SLICERS];
  int32_t adc_min;
  int32_t adc_max;
  float m_amp;
  float s_amp;
  uint8_t dcd;
  uint8_t last_slicer; /* which slicer last contributed an OK frame */
} afsk_aplus_dbg_t;

void afsk_aplus_dbg_get(afsk_aplus_dbg_t *out);
void afsk_aplus_dbg_clear(void);

#ifdef __cplusplus
}
#endif

#endif
