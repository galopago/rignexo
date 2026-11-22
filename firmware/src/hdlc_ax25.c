#include "hdlc_ax25.h"

#include <stdbool.h>
#include <string.h>

#define HDLC_RX_BUF_MAX 514U /* 512 body + 2 FCS */

uint16_t hdlc_crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFFu;
  if (data == NULL) {
    return (uint16_t)~crc;
  }

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1u) {
        crc = (uint16_t)((crc >> 1) ^ 0x8408u);
      } else {
        crc = (uint16_t)(crc >> 1);
      }
    }
  }
  return (uint16_t)~crc;
}

typedef struct {
  uint8_t *out;
  size_t cap;
  size_t nbits;
  unsigned ones;
} bit_writer_t;

static bool bw_push(bit_writer_t *w, unsigned bit, bool stuff) {
  if (w->nbits >= w->cap) {
    return false;
  }
  w->out[w->nbits++] = (uint8_t)(bit & 1u);

  if (!stuff) {
    w->ones = 0;
    return true;
  }

  if (bit) {
    w->ones++;
    if (w->ones == 5u) {
      if (w->nbits >= w->cap) {
        return false;
      }
      w->out[w->nbits++] = 0;
      w->ones = 0;
    }
  } else {
    w->ones = 0;
  }
  return true;
}

static bool bw_byte_raw(bit_writer_t *w, uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    if (!bw_push(w, (byte >> i) & 1u, false)) {
      return false;
    }
  }
  return true;
}

static bool bw_byte_stuffed(bit_writer_t *w, uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    if (!bw_push(w, (byte >> i) & 1u, true)) {
      return false;
    }
  }
  return true;
}

size_t hdlc_tx_build_bits(const uint8_t *payload, size_t payload_len,
                          unsigned opening_flags, unsigned closing_flags,
                          uint8_t *out_bits, size_t out_cap) {
  if (out_bits == NULL || out_cap == 0) {
    return 0;
  }
  if (payload_len > 0 && payload == NULL) {
    return 0;
  }

  bit_writer_t w = {
      .out = out_bits,
      .cap = out_cap,
      .nbits = 0,
      .ones = 0,
  };

  for (unsigned i = 0; i < opening_flags; i++) {
    if (!bw_byte_raw(&w, 0x7Eu)) {
      return 0;
    }
  }

  uint16_t fcs = hdlc_crc16(payload, payload_len);
  for (size_t i = 0; i < payload_len; i++) {
    if (!bw_byte_stuffed(&w, payload[i])) {
      return 0;
    }
  }
  if (!bw_byte_stuffed(&w, (uint8_t)(fcs & 0xFFu)) ||
      !bw_byte_stuffed(&w, (uint8_t)((fcs >> 8) & 0xFFu))) {
    return 0;
  }

  w.ones = 0;
  for (unsigned i = 0; i < closing_flags; i++) {
    if (!bw_byte_raw(&w, 0x7Eu)) {
      return 0;
    }
  }

  return w.nbits;
}

//--------------------------------------------------------------------+
// HDLC RX streaming (LSB-first, matches TX builder)
//--------------------------------------------------------------------+

static uint8_t s_rx_buf[HDLC_RX_BUF_MAX];
static size_t s_rx_len;
static uint8_t s_rx_byte;
static unsigned s_rx_bitcount;
static unsigned s_rx_ones;
static bool s_rx_in_frame;

static hdlc_rx_dbg_t s_dbg;

void hdlc_rx_dbg_get(hdlc_rx_dbg_t *out) {
  if (out) {
    *out = s_dbg;
  }
}

void hdlc_rx_dbg_clear(void) {
  memset(&s_dbg, 0, sizeof(s_dbg));
}

void hdlc_rx_reset(void) {
  s_rx_len = 0;
  s_rx_byte = 0;
  s_rx_bitcount = 0;
  s_rx_ones = 0;
  s_rx_in_frame = false;
}

static void hdlc_rx_retract_bits(unsigned n) {
  while (n > 0u) {
    if (s_rx_bitcount == 0u) {
      if (s_rx_len == 0u) {
        return;
      }
      s_rx_len--;
      s_rx_byte = s_rx_buf[s_rx_len];
      s_rx_bitcount = 8u;
    }
    s_rx_bitcount--;
    s_rx_byte = (uint8_t)(s_rx_byte & (uint8_t)((1u << s_rx_bitcount) - 1u));
    n--;
  }
}

static void hdlc_rx_accumulate(unsigned bit) {
  s_rx_byte = (uint8_t)(s_rx_byte | (bit << s_rx_bitcount));
  s_rx_bitcount++;
  if (s_rx_bitcount >= 8u) {
    if (s_rx_len < HDLC_RX_BUF_MAX) {
      s_rx_buf[s_rx_len++] = s_rx_byte;
    }
    s_rx_byte = 0;
    s_rx_bitcount = 0;
  }
}

static hdlc_rx_event_t hdlc_rx_deliver(const uint8_t **payload, size_t *len) {
  if (s_rx_bitcount != 0u) {
    s_dbg.align_slip++;
    return HDLC_RX_NONE;
  }
  if (s_rx_len < 2u) {
    return HDLC_RX_NONE;
  }
  if (s_rx_len > HDLC_RX_BUF_MAX) {
    s_dbg.abort++;
    return HDLC_RX_ABORT;
  }

  const size_t body_len = s_rx_len - 2u;
  if (body_len > 512u) {
    s_dbg.abort++;
    return HDLC_RX_ABORT;
  }

  const uint16_t calc = hdlc_crc16(s_rx_buf, body_len);
  const uint16_t got =
      (uint16_t)s_rx_buf[body_len] | ((uint16_t)s_rx_buf[body_len + 1u] << 8);

  if (calc != got) {
    s_dbg.bad_fcs++;
    return HDLC_RX_FRAME_BAD_FCS;
  }

  if (payload) {
    *payload = s_rx_buf;
  }
  if (len) {
    *len = body_len;
  }
  s_dbg.ok++;
  return HDLC_RX_FRAME_OK;
}

hdlc_rx_event_t hdlc_rx_bit(unsigned bit, const uint8_t **payload, size_t *len) {
  bit &= 1u;
  if (payload) {
    *payload = NULL;
  }
  if (len) {
    *len = 0;
  }

  s_dbg.bits++;

  if (bit) {
    s_rx_ones++;
    if (s_rx_ones > 6u) {
      hdlc_rx_reset();
      s_dbg.abort++;
      return HDLC_RX_ABORT;
    }
    /* Hold 6th one — closing flag/abort decided on next bit. */
    if (s_rx_in_frame && s_rx_ones < 6u) {
      hdlc_rx_accumulate(1u);
      if (s_rx_len >= HDLC_RX_BUF_MAX && s_rx_bitcount == 0u) {
        hdlc_rx_reset();
        s_dbg.abort++;
        return HDLC_RX_ABORT;
      }
    }
    return HDLC_RX_NONE;
  }

  /* bit == 0 */
  if (s_rx_ones == 5u) {
    /* Stuffed 0 after five 1s — discard. */
    s_rx_ones = 0;
    return HDLC_RX_NONE;
  }

  if (s_rx_ones == 6u) {
    /* Flag 01111110: leading 0 + five 1s were accumulated; retract them. */
    hdlc_rx_event_t ev = HDLC_RX_NONE;
    if (s_rx_in_frame) {
      hdlc_rx_retract_bits(6u);
      ev = hdlc_rx_deliver(payload, len);
    }
    s_rx_ones = 0;
    s_rx_in_frame = true;
    s_rx_len = 0;
    s_rx_byte = 0;
    s_rx_bitcount = 0;
    return ev;
  }

  s_rx_ones = 0;
  if (s_rx_in_frame) {
    hdlc_rx_accumulate(0u);
    if (s_rx_len >= HDLC_RX_BUF_MAX && s_rx_bitcount == 0u) {
      hdlc_rx_reset();
      s_dbg.abort++;
      return HDLC_RX_ABORT;
    }
  }
  return HDLC_RX_NONE;
}
