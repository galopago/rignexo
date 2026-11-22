/**
 * Copyright (c) 2021 JN1DFF (Kazuhisa Yokota)
 * Ported for RIGNEXO RX-only (D28) from amedes/pico_tnc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TNC_RX_TYPES_H
#define TNC_RX_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "filter.h"

#define PORT_N 1
#define BAUD_RATE 1200
#define SAMPLING_N 11
#define SAMPLING_RATE (BAUD_RATE * SAMPLING_N) /* 13200 */
#define DELAY_US 446
#define DELAYED_N ((SAMPLING_RATE * DELAY_US + 500000) / 1000000) /* 6 */

#define DATA_LEN 512
#define FIR_LPF_N 27
#define FIR_BPF_N 25

/* D29: match pico_tnc default — feed 0…255 (adc_read()>>4 from 12-bit). */
#define ADC_BIT 8

#define DECODE_PLL 1

enum RX_STATE {
  FLAG = 0,
  DATA
};

typedef struct TNC_RX {
  uint8_t bit;

  uint16_t data_cnt;
  uint8_t data[DATA_LEN];
  uint8_t state;
  uint8_t flag;
  uint8_t data_byte;
  uint8_t data_bit_cnt;

  int32_t pll_counter;
  uint8_t pval;
  uint8_t nrzi;

  int delayed[DELAYED_N];
  int delay_idx;
  int cdt;
  int cdt_lvl;
  int avg;

  filter_t lpf;
  filter_t bpf;

  /* Debug tallies (RIGNEXO STAT mapping). */
  uint32_t dbg_bits;
  uint32_t dbg_ok;
  uint32_t dbg_bad_fcs;
  uint32_t dbg_abort;
  uint32_t dbg_disc_tr;
} tnc_rx_t;

#endif
