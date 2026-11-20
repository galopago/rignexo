#include "cat_kenwood.h"

#include "ptt.h"

#include "tusb.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CAT_CMD_MAX 128

typedef struct {
  uint32_t vfo_a_hz;
  uint32_t vfo_b_hz;
  uint8_t md_code;
  uint16_t fw_hz;
  uint8_t sl_val;
  uint8_t sh_val;
  uint8_t pc_pct;
  uint8_t fr_vfo;
  uint8_t ft_vfo;
  uint8_t ai_mode;
  uint8_t rg_val;
  uint8_t ag_val;
  uint16_t sq_val;
} cat_soft_state_t;

static cat_soft_state_t s_st;
static char s_buf[CAT_CMD_MAX];
static size_t s_len;
static char s_reply[64];

static void cat_soft_defaults(void) {
  s_st.vfo_a_hz = 14074000u;
  s_st.vfo_b_hz = 14074000u;
  s_st.md_code = 2;
  s_st.fw_hz = 2700;
  s_st.sl_val = 0;
  s_st.sh_val = 8;
  s_st.pc_pct = 100;
  s_st.fr_vfo = 0;
  s_st.ft_vfo = 0;
  s_st.ai_mode = 0;
  s_st.rg_val = 100;
  s_st.ag_val = 100;
  s_st.sq_val = 0;
}

static void cat_reply(const char *s) {
  if (s == NULL || s[0] == '\0') {
    return;
  }
  tud_cdc_write(s, strlen(s));
  tud_cdc_write_flush();
}

static bool cmd_eq(const char *cmd, size_t len, const char *lit) {
  const size_t n = strlen(lit);
  return len == n && memcmp(cmd, lit, n) == 0;
}

static bool cmd_starts(const char *cmd, size_t len, const char *prefix) {
  const size_t n = strlen(prefix);
  return len >= n && memcmp(cmd, prefix, n) == 0;
}

static bool all_digits(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  return n > 0;
}

static uint32_t parse_u32(const char *s, size_t n) {
  uint32_t v = 0;
  for (size_t i = 0; i < n; i++) {
    v = (v * 10u) + (uint32_t)(s[i] - '0');
  }
  return v;
}

static void reply_fa(void) {
  snprintf(s_reply, sizeof(s_reply), "FA%011lu;", (unsigned long)s_st.vfo_a_hz);
  cat_reply(s_reply);
}

static void reply_fb(void) {
  snprintf(s_reply, sizeof(s_reply), "FB%011lu;", (unsigned long)s_st.vfo_b_hz);
  cat_reply(s_reply);
}

static void reply_md(void) {
  snprintf(s_reply, sizeof(s_reply), "MD%u;", (unsigned)s_st.md_code);
  cat_reply(s_reply);
}

static void reply_fw(void) {
  /* Encode as 7 digits to match cold-start FA style (FW0002700;). */
  snprintf(s_reply, sizeof(s_reply), "FW%07u;", (unsigned)s_st.fw_hz);
  cat_reply(s_reply);
}

static void reply_sl(void) {
  snprintf(s_reply, sizeof(s_reply), "SL%02u;", (unsigned)s_st.sl_val);
  cat_reply(s_reply);
}

static void reply_sh(void) {
  snprintf(s_reply, sizeof(s_reply), "SH%02u;", (unsigned)s_st.sh_val);
  cat_reply(s_reply);
}

static void reply_pc(void) {
  snprintf(s_reply, sizeof(s_reply), "PC%03u;", (unsigned)s_st.pc_pct);
  cat_reply(s_reply);
}

static void reply_fr(void) {
  snprintf(s_reply, sizeof(s_reply), "FR%u;", (unsigned)s_st.fr_vfo);
  cat_reply(s_reply);
}

static void reply_ft(void) {
  snprintf(s_reply, sizeof(s_reply), "FT%u;", (unsigned)s_st.ft_vfo);
  cat_reply(s_reply);
}

static void reply_ai(void) {
  snprintf(s_reply, sizeof(s_reply), "AI%u;", (unsigned)s_st.ai_mode);
  cat_reply(s_reply);
}

static void reply_rg(void) {
  snprintf(s_reply, sizeof(s_reply), "RG%03u;", (unsigned)s_st.rg_val);
  cat_reply(s_reply);
}

static void reply_ag(void) {
  snprintf(s_reply, sizeof(s_reply), "AG0%03u;", (unsigned)s_st.ag_val);
  cat_reply(s_reply);
}

static void reply_sq(void) {
  snprintf(s_reply, sizeof(s_reply), "SQ%03u;", (unsigned)s_st.sq_val);
  cat_reply(s_reply);
}

static void reply_if(void) {
  /*
   * Hamlib TS-2000 IF frame (D20 / RGNX_FW_CAT_IF_HAMLIB_V1):
   * wire length exactly 38 including ';'; payload 37 after strip.
   * Cold-start idle: IF00014074000      000000000020000000;
   */
  uint32_t hz = s_st.vfo_a_hz;
  if (s_st.fr_vfo == 1) {
    hz = s_st.vfo_b_hz;
  }

  char body[38]; /* 37 payload + NUL; ';' appended into s_reply */
  /* 0-1 opcode */
  body[0] = 'I';
  body[1] = 'F';
  /* 2-12 frequency, 11 digits */
  for (int i = 10; i >= 0; i--) {
    body[2 + i] = (char)('0' + (hz % 10u));
    hz /= 10u;
  }
  /* 13-18 six spaces */
  for (int i = 13; i <= 18; i++) {
    body[i] = ' ';
  }
  /* 19-24 RIT digits (no +/- sign) */
  for (int i = 19; i <= 24; i++) {
    body[i] = '0';
  }
  body[25] = '0'; /* RIT on/off */
  body[26] = '0'; /* XIT on/off */
  body[27] = '0'; /* memory padding */
  body[28] = ptt_is_active() ? '1' : '0'; /* TX / PTT — Hamlib info[28] */
  body[29] = (char)('0' + (s_st.md_code % 10u)); /* MD — info[29] */
  body[30] = (char)('0' + (s_st.fr_vfo % 10u));  /* VFO — info[30] */
  body[31] = '0';
  body[32] = (s_st.fr_vfo != s_st.ft_vfo) ? '1' : '0'; /* split — info[32] */
  body[33] = '0';
  body[34] = '0';
  body[35] = '0';
  body[36] = '0';
  body[37] = '\0';

  /* strlen(body) MUST be 37 */
  snprintf(s_reply, sizeof(s_reply), "%s;", body);
  cat_reply(s_reply);
}

/* Set: payload after opcode; returns false → reply ?; and no store. */
static bool handle_set_digits(const char *payload, size_t plen, size_t expect,
                              uint32_t *out, uint32_t max_val) {
  if (plen != expect || !all_digits(payload, plen)) {
    return false;
  }
  uint32_t v = parse_u32(payload, plen);
  if (v > max_val) {
    return false;
  }
  *out = v;
  return true;
}

static void handle_command(const char *cmd, size_t len) {
  while (len > 0 && (cmd[0] == ' ' || cmd[0] == '\t')) {
    cmd++;
    len--;
  }
  if (len == 0) {
    return;
  }

  /* PTT — no soft-state, no reply body. */
  if (cmd_eq(cmd, len, "TX") || cmd_eq(cmd, len, "TX0") || cmd_eq(cmd, len, "TX1")) {
    ptt_set_active(true);
    return;
  }
  if (cmd_eq(cmd, len, "RX")) {
    ptt_set_active(false);
    return;
  }

  if (cmd_eq(cmd, len, "ID")) {
    cat_reply("ID019;");
    return;
  }

  if (cmd_eq(cmd, len, "SM")) {
    cat_reply("SM00000;");
    return;
  }

  if (cmd_eq(cmd, len, "IF")) {
    reply_if();
    return;
  }

  /* ---- FA ---- */
  if (cmd_eq(cmd, len, "FA")) {
    reply_fa();
    return;
  }
  if (cmd_starts(cmd, len, "FA")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 11, &v, UINT32_MAX)) {
      cat_reply("?;");
      return;
    }
    s_st.vfo_a_hz = v;
    return; /* silent set ack */
  }

  /* ---- FB ---- */
  if (cmd_eq(cmd, len, "FB")) {
    reply_fb();
    return;
  }
  if (cmd_starts(cmd, len, "FB")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 11, &v, UINT32_MAX)) {
      cat_reply("?;");
      return;
    }
    s_st.vfo_b_hz = v;
    return;
  }

  /* ---- MD ---- */
  if (cmd_eq(cmd, len, "MD")) {
    reply_md();
    return;
  }
  if (cmd_starts(cmd, len, "MD")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 1, &v, 9u)) {
      cat_reply("?;");
      return;
    }
    s_st.md_code = (uint8_t)v;
    return;
  }

  /* ---- FW (4 or 7 digit payloads accepted; store Hz) ---- */
  if (cmd_eq(cmd, len, "FW")) {
    reply_fw();
    return;
  }
  if (cmd_starts(cmd, len, "FW")) {
    const char *p = cmd + 2;
    size_t plen = len - 2;
    if (!(plen == 4 || plen == 7) || !all_digits(p, plen)) {
      cat_reply("?;");
      return;
    }
    uint32_t v = parse_u32(p, plen);
    if (v > 9999u) {
      cat_reply("?;");
      return;
    }
    s_st.fw_hz = (uint16_t)v;
    return;
  }

  /* ---- SL ---- */
  if (cmd_eq(cmd, len, "SL")) {
    reply_sl();
    return;
  }
  if (cmd_starts(cmd, len, "SL")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 2, &v, 99u)) {
      cat_reply("?;");
      return;
    }
    s_st.sl_val = (uint8_t)v;
    return;
  }

  /* ---- SH ---- */
  if (cmd_eq(cmd, len, "SH")) {
    reply_sh();
    return;
  }
  if (cmd_starts(cmd, len, "SH")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 2, &v, 99u)) {
      cat_reply("?;");
      return;
    }
    s_st.sh_val = (uint8_t)v;
    return;
  }

  /* ---- PC ---- */
  if (cmd_eq(cmd, len, "PC")) {
    reply_pc();
    return;
  }
  if (cmd_starts(cmd, len, "PC")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 3, &v, 100u)) {
      cat_reply("?;");
      return;
    }
    s_st.pc_pct = (uint8_t)v;
    return;
  }

  /* ---- FR / FT ---- */
  if (cmd_eq(cmd, len, "FR")) {
    reply_fr();
    return;
  }
  if (cmd_starts(cmd, len, "FR")) {
    uint32_t v;
    /* 0=A, 1=B, 2=MEM — Hamlib IF info[30] */
    if (!handle_set_digits(cmd + 2, len - 2, 1, &v, 2u)) {
      cat_reply("?;");
      return;
    }
    s_st.fr_vfo = (uint8_t)v;
    return;
  }
  if (cmd_eq(cmd, len, "FT")) {
    reply_ft();
    return;
  }
  if (cmd_starts(cmd, len, "FT")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 1, &v, 1u)) {
      cat_reply("?;");
      return;
    }
    s_st.ft_vfo = (uint8_t)v;
    return;
  }

  /* ---- AI ---- */
  if (cmd_eq(cmd, len, "AI")) {
    reply_ai();
    return;
  }
  if (cmd_starts(cmd, len, "AI")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 1, &v, 2u)) {
      cat_reply("?;");
      return;
    }
    s_st.ai_mode = (uint8_t)v;
    return;
  }

  /* ---- RG ---- */
  if (cmd_eq(cmd, len, "RG")) {
    reply_rg();
    return;
  }
  if (cmd_starts(cmd, len, "RG")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 3, &v, 255u)) {
      cat_reply("?;");
      return;
    }
    s_st.rg_val = (uint8_t)v;
    return;
  }

  /* ---- AG / AG0 ---- */
  if (cmd_eq(cmd, len, "AG") || cmd_eq(cmd, len, "AG0")) {
    reply_ag();
    return;
  }
  if (cmd_starts(cmd, len, "AG0")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 3, len - 3, 3, &v, 255u)) {
      cat_reply("?;");
      return;
    }
    s_st.ag_val = (uint8_t)v;
    return;
  }
  if (cmd_starts(cmd, len, "AG")) {
    uint32_t v;
    /* AG100; or AG0100; */
    size_t plen = len - 2;
    if (plen == 4 && cmd[2] == '0') {
      if (!handle_set_digits(cmd + 3, 3, 3, &v, 255u)) {
        cat_reply("?;");
        return;
      }
    } else if (!handle_set_digits(cmd + 2, plen, 3, &v, 255u)) {
      cat_reply("?;");
      return;
    }
    s_st.ag_val = (uint8_t)v;
    return;
  }

  /* ---- SQ ---- */
  if (cmd_eq(cmd, len, "SQ")) {
    reply_sq();
    return;
  }
  if (cmd_starts(cmd, len, "SQ")) {
    uint32_t v;
    if (!handle_set_digits(cmd + 2, len - 2, 3, &v, 255u)) {
      cat_reply("?;");
      return;
    }
    s_st.sq_val = (uint16_t)v;
    return;
  }

  cat_reply("?;");
}

void cat_kenwood_init(void) {
  s_len = 0;
  cat_soft_defaults();
}

void cat_kenwood_feed(const uint8_t *data, size_t len) {
  if (data == NULL || len == 0) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    const char c = (char)data[i];

    if (c == '\r' || c == '\n') {
      continue;
    }

    if (c == ';') {
      handle_command(s_buf, s_len);
      s_len = 0;
      continue;
    }

    if (s_len >= CAT_CMD_MAX - 1) {
      s_len = 0;
      cat_reply("?;");
      continue;
    }

    s_buf[s_len++] = c;
  }
}
