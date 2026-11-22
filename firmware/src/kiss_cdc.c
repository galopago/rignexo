#include "kiss_cdc.h"

#include "afsk_bell202.h"
#include "board_config.h"

#include "tusb.h"

#include <stdbool.h>
#include <string.h>

#define KISS_FEND  0xC0u
#define KISS_FESC  0xDBu
#define KISS_TFEND 0xDCu
#define KISS_TFESC 0xDDu

#define KISS_CMD_DATA       0x00u
#define KISS_CMD_TXDELAY    0x01u
#define KISS_CMD_PERSIST    0x02u
#define KISS_CMD_SLOTTIME   0x03u
#define KISS_CMD_TXTAIL     0x04u
#define KISS_CMD_FULLDUPLEX 0x05u
#define KISS_CMD_DEBUG_STAT 0x0Cu
#define KISS_CMD_DEBUG_CLR  0x0Du
#define KISS_CMD_RETURN     0xFFu

#define KISS_FRAME_MAX 512
#define ASCII_LINE_MAX 24

static uint8_t s_txdelay;
static uint8_t s_persist;
static uint8_t s_slottime;
static uint8_t s_txtail;
static uint8_t s_fullduplex;

static uint8_t s_frame[KISS_FRAME_MAX];
static size_t s_frame_len;
static bool s_in_frame;
static bool s_escape;

static char s_ascii[ASCII_LINE_MAX];
static size_t s_ascii_len;

static uint32_t s_kiss_ok;
static uint32_t s_kiss_drop;

static void kiss_apply_command(uint8_t cmd, const uint8_t *payload, size_t len) {
  const uint8_t code = (uint8_t)(cmd & 0x0Fu);

  if (cmd == KISS_CMD_RETURN) {
    return;
  }

  switch (code) {
    case KISS_CMD_DATA:
      (void)afsk_tx_queue_frame(payload, len);
      break;
    case KISS_CMD_TXDELAY:
      if (len >= 1) {
        s_txdelay = payload[0];
      }
      break;
    case KISS_CMD_PERSIST:
      if (len >= 1) {
        s_persist = payload[0];
      }
      break;
    case KISS_CMD_SLOTTIME:
      if (len >= 1) {
        s_slottime = payload[0];
      }
      break;
    case KISS_CMD_TXTAIL:
      if (len >= 1) {
        s_txtail = payload[0];
      }
      break;
    case KISS_CMD_FULLDUPLEX:
      if (len >= 1) {
        s_fullduplex = payload[0];
      }
      break;
    case KISS_CMD_DEBUG_STAT:
      afsk_rx_debug_request_dump();
      break;
    case KISS_CMD_DEBUG_CLR:
      afsk_rx_debug_clear();
      break;
    default:
      break;
  }
}

static void kiss_end_frame(void) {
  if (s_frame_len == 0) {
    s_in_frame = false;
    s_escape = false;
    return;
  }

  const uint8_t cmd = s_frame[0];
  const uint8_t *payload = s_frame + 1;
  const size_t plen = s_frame_len - 1;
  kiss_apply_command(cmd, payload, plen);

  s_frame_len = 0;
  s_in_frame = false;
  s_escape = false;
}

static void kiss_ascii_end_line(void) {
  if (s_ascii_len == 0) {
    return;
  }
  s_ascii[s_ascii_len] = '\0';
  if (strcmp(s_ascii, "RGNX STAT") == 0) {
    afsk_rx_debug_request_dump();
  } else if (strcmp(s_ascii, "RGNX CLR") == 0) {
    afsk_rx_debug_clear();
  }
  s_ascii_len = 0;
}

static void kiss_ascii_feed(uint8_t b) {
  if (b == '\n' || b == '\r') {
    kiss_ascii_end_line();
    return;
  }
  if (s_ascii_len + 1u >= ASCII_LINE_MAX) {
    s_ascii_len = 0;
    return;
  }
  s_ascii[s_ascii_len++] = (char)b;
}

static bool kiss_write_byte(uint8_t b) {
  if (!tud_cdc_connected()) {
    return false;
  }
  if (tud_cdc_write_available() < 1) {
    return false;
  }
  return tud_cdc_write_char(b) == 1;
}

static bool kiss_write_escaped(uint8_t b) {
  if (b == KISS_FEND) {
    return kiss_write_byte(KISS_FESC) && kiss_write_byte(KISS_TFEND);
  }
  if (b == KISS_FESC) {
    return kiss_write_byte(KISS_FESC) && kiss_write_byte(KISS_TFESC);
  }
  return kiss_write_byte(b);
}

void kiss_cdc_init(void) {
  s_txdelay = (uint8_t)BOARD_KISS_TXDELAY;
  s_persist = (uint8_t)BOARD_KISS_PERSIST;
  s_slottime = (uint8_t)BOARD_KISS_SLOTTIME;
  s_txtail = (uint8_t)BOARD_KISS_TXTAIL;
  s_fullduplex = 0;
  s_frame_len = 0;
  s_in_frame = false;
  s_escape = false;
  s_ascii_len = 0;
  s_kiss_ok = 0;
  s_kiss_drop = 0;
}

void kiss_cdc_feed(const uint8_t *data, size_t len) {
  if (data == NULL || len == 0) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    const uint8_t b = data[i];

    if (b == KISS_FEND) {
      s_ascii_len = 0; /* abandon ASCII line if any */
      if (s_in_frame) {
        kiss_end_frame();
      }
      s_in_frame = true;
      s_escape = false;
      s_frame_len = 0;
      continue;
    }

    if (!s_in_frame) {
      /* Outside KISS: host ASCII debug lines (RGNX STAT / CLR). */
      kiss_ascii_feed(b);
      continue;
    }

    uint8_t out = b;
    if (s_escape) {
      s_escape = false;
      if (b == KISS_TFEND) {
        out = KISS_FEND;
      } else if (b == KISS_TFESC) {
        out = KISS_FESC;
      } else {
        s_in_frame = false;
        s_frame_len = 0;
        continue;
      }
    } else if (b == KISS_FESC) {
      s_escape = true;
      continue;
    }

    if (s_frame_len >= KISS_FRAME_MAX) {
      s_in_frame = false;
      s_frame_len = 0;
      s_escape = false;
      continue;
    }

    s_frame[s_frame_len++] = out;
  }
}

void kiss_cdc_send_frame(const uint8_t *payload, size_t len) {
  if (payload == NULL || len == 0 || len > KISS_FRAME_MAX) {
    s_kiss_drop++;
    return;
  }
  if (!tud_cdc_connected()) {
    s_kiss_drop++;
    return;
  }

  if (!kiss_write_byte(KISS_FEND) || !kiss_write_byte(KISS_CMD_DATA)) {
    s_kiss_drop++;
    return;
  }
  for (size_t i = 0; i < len; i++) {
    if (!kiss_write_escaped(payload[i])) {
      tud_cdc_write_flush();
      s_kiss_drop++;
      return;
    }
  }
  if (!kiss_write_byte(KISS_FEND)) {
    s_kiss_drop++;
    tud_cdc_write_flush();
    return;
  }
  tud_cdc_write_flush();
  s_kiss_ok++;
}

uint8_t kiss_cdc_txdelay(void) {
  return s_txdelay;
}

uint8_t kiss_cdc_txtail(void) {
  return s_txtail;
}

uint8_t kiss_cdc_persist(void) {
  return s_persist;
}

uint8_t kiss_cdc_slottime(void) {
  return s_slottime;
}

uint8_t kiss_cdc_fullduplex(void) {
  return s_fullduplex;
}

uint32_t kiss_cdc_dbg_kiss_ok(void) {
  return s_kiss_ok;
}

uint32_t kiss_cdc_dbg_kiss_drop(void) {
  return s_kiss_drop;
}

void kiss_cdc_dbg_note_drop(void) {
  s_kiss_drop++;
}

void kiss_cdc_dbg_clear(void) {
  s_kiss_ok = 0;
  s_kiss_drop = 0;
}
