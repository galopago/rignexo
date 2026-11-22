/*
 * Host offline golden for D31 A+ demod (O0/O1).
 * Links the same src/afsk_aplus + hdlc_ax25 sources against a WAV fixture.
 */

#include "afsk_aplus.h"
#include "board_config.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint8_t payloads[64][256];
  size_t lens[64];
  unsigned n;
} frames_t;

static frames_t s_frames;

static void on_frame(const uint8_t *payload, size_t len, void *user) {
  frames_t *f = (frames_t *)user;
  if (f->n >= 64 || len > 255) {
    return;
  }
  memcpy(f->payloads[f->n], payload, len);
  f->lens[f->n] = len;
  f->n++;
}

static int read_wav_mono_s16(const char *path, int16_t **out, unsigned *nframes,
                             unsigned *rate) {
  FILE *fp = fopen(path, "rb");
  uint8_t hdr[44];
  uint16_t fmt;
  uint16_t ch;
  uint32_t sr;
  uint16_t bps;
  uint32_t data_sz;
  size_t nsamp;
  int16_t *buf;
  long pos;

  if (!fp) {
    return -1;
  }
  if (fread(hdr, 1, 44, fp) != 44) {
    fclose(fp);
    return -2;
  }
  /* Minimal PCM WAV: seek "data" chunk if needed. */
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    fclose(fp);
    return -3;
  }
  fmt = (uint16_t)hdr[20] | ((uint16_t)hdr[21] << 8);
  ch = (uint16_t)hdr[22] | ((uint16_t)hdr[23] << 8);
  sr = (uint32_t)hdr[24] | ((uint32_t)hdr[25] << 8) | ((uint32_t)hdr[26] << 16) |
       ((uint32_t)hdr[27] << 24);
  bps = (uint16_t)hdr[34] | ((uint16_t)hdr[35] << 8);

  pos = 12;
  fseek(fp, 12, SEEK_SET);
  data_sz = 0;
  for (;;) {
    uint8_t chdr[8];
    uint32_t csz;
    if (fread(chdr, 1, 8, fp) != 8) {
      fclose(fp);
      return -4;
    }
    csz = (uint32_t)chdr[4] | ((uint32_t)chdr[5] << 8) |
          ((uint32_t)chdr[6] << 16) | ((uint32_t)chdr[7] << 24);
    if (memcmp(chdr, "data", 4) == 0) {
      data_sz = csz;
      break;
    }
    fseek(fp, (long)csz + (csz & 1u), SEEK_CUR);
    (void)pos;
  }

  if (fmt != 1 || ch != 1 || bps != 16) {
    fclose(fp);
    return -5;
  }
  nsamp = data_sz / 2u;
  buf = (int16_t *)malloc(nsamp * sizeof(int16_t));
  if (!buf || fread(buf, 2, nsamp, fp) != nsamp) {
    free(buf);
    fclose(fp);
    return -6;
  }
  fclose(fp);
  *out = buf;
  *nframes = (unsigned)nsamp;
  *rate = sr;
  return 0;
}

static void run_scale(const int16_t *pcm48, unsigned n48, float scale,
                      unsigned *ok_out) {
  unsigned i;
  afsk_aplus_dbg_t d;

  memset(&s_frames, 0, sizeof(s_frames));
  afsk_aplus_init(on_frame, &s_frames);

  /* Decimate 48 kHz → 24 kHz (average pair — anti-alias lite). */
  for (i = 0; i + 1u < n48; i += 2u) {
    float x =
        (((float)pcm48[i] + (float)pcm48[i + 1u]) * 0.5f / 32768.f) * scale;
    afsk_aplus_process(x);
  }

  afsk_aplus_dbg_get(&d);
  *ok_out = d.ok;
  printf("scale=%.2f ok=%u bad_fcs=%u flag=%u entered=%u bits=%u frames_cb=%u "
         "dup=%u last_sl=%u\n",
         scale, d.ok, d.bad_fcs, d.flag_hunt, d.entered_data, d.bits, s_frames.n,
         d.dup_drop, d.last_slicer);
  if (s_frames.n > 0) {
    unsigned j;
    unsigned show = s_frames.n < 5u ? s_frames.n : 5u;
    for (j = 0; j < show; j++) {
      unsigned k;
      printf("  frame[%u] len=%zu ascii=\"", j, s_frames.lens[j]);
      for (k = 0; k < s_frames.lens[j]; k++) {
        unsigned char c = s_frames.payloads[j][k];
        putchar((c >= 32 && c < 127) ? (char)c : '.');
      }
      printf("\"\n");
    }
  }
}

int main(int argc, char **argv) {
  const char *path =
      "agent-workspaces/tester/fixtures/afsk_rx_bench_2min.wav";
  int16_t *pcm = NULL;
  unsigned n = 0;
  unsigned rate = 0;
  unsigned ok0 = 0;
  unsigned ok_q = 0;
  unsigned ok_l = 0;
  int rc;

  if (argc >= 2) {
    path = argv[1];
  }

  rc = read_wav_mono_s16(path, &pcm, &n, &rate);
  if (rc != 0) {
    fprintf(stderr, "WAV open failed (%d): %s\n", rc, path);
    return 2;
  }
  printf("WAV %s rate=%u samples=%u → RX %u Hz A+ N=%d\n", path, rate, n,
         (unsigned)BOARD_AFSK_RX_SAMPLE_RATE_HZ, AFSK_APLUS_N_SLICERS);
  if (rate != 48000u) {
    fprintf(stderr, "expected 48000 Hz fixture\n");
    free(pcm);
    return 3;
  }

  printf("--- O0 nominal ---\n");
  run_scale(pcm, n, 1.0f, &ok0);
  printf("--- O1 quiet 0.25x ---\n");
  run_scale(pcm, n, 0.25f, &ok_q);
  printf("--- O1 loud 2.0x ---\n");
  run_scale(pcm, n, 2.0f, &ok_l);

  free(pcm);

  printf("RESULT O0_ok=%u O1_quiet_ok=%u O1_loud_ok=%u\n", ok0, ok_q, ok_l);
  if (ok0 >= 1u && ok_q >= 1u && ok_l >= 1u) {
    printf("PASS O0+O1\n");
    return 0;
  }
  printf("FAIL need ok>=1 on O0 and both O1 scales\n");
  return 1;
}
