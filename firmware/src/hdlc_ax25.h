#ifndef HDLC_AX25_H
#define HDLC_AX25_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  HDLC_RX_NONE = 0,
  HDLC_RX_ABORT,
  HDLC_RX_FRAME_OK,
  HDLC_RX_FRAME_BAD_FCS,
} hdlc_rx_event_t;

/* CRC-16/X.25 (AX.25 FCS), init 0xFFFF, final XOR 0xFFFF. */
uint16_t hdlc_crc16(const uint8_t *data, size_t len);

/*
 * Build TX bit stream: opening_flags × 0x7E, then stuffed payload+FCS (LE),
 * then closing_flags × 0x7E. Bits LSB-first, one bit per out_bits[] byte.
 */
size_t hdlc_tx_build_bits(const uint8_t *payload, size_t payload_len,
                          unsigned opening_flags, unsigned closing_flags,
                          uint8_t *out_bits, size_t out_cap);

void hdlc_rx_reset(void);

/*
 * Feed one HDLC bit (after NRZI decode). On FRAME_OK / FRAME_BAD_FCS,
 * *payload / *len point at assembled bytes without FCS when OK.
 * Pointer valid until next hdlc_rx_bit / hdlc_rx_reset.
 */
hdlc_rx_event_t hdlc_rx_bit(unsigned bit, const uint8_t **payload, size_t *len);

/* D25 since-clear HDLC tallies (wrap freely). */
typedef struct {
  uint32_t bits;
  uint32_t abort;
  uint32_t bad_fcs;
  uint32_t ok;
  uint32_t align_slip;
} hdlc_rx_dbg_t;

void hdlc_rx_dbg_get(hdlc_rx_dbg_t *out);
void hdlc_rx_dbg_clear(void);

#ifdef __cplusplus
}
#endif

#endif
