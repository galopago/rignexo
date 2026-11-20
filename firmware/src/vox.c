#include "vox.h"

#include "board_config.h"
#include "mode_store.h"
#include "ptt.h"

#include "pico/stdlib.h"

static bool s_enabled;
static bool s_ptt_on;
static uint32_t s_last_activity_ms;
static uint32_t s_active_run;

void vox_init(void) {
  s_enabled = device_mode_is_audio_vox();
  s_ptt_on = false;
  s_last_activity_ms = 0;
  s_active_run = 0;
  if (s_enabled) {
    ptt_force_inactive();
  }
}

void vox_process_pcm16(const int16_t *samples, size_t count) {
  if (!s_enabled || samples == NULL || count == 0) {
    return;
  }

  uint32_t hit = 0;
  for (size_t i = 0; i < count; i++) {
    const int32_t a = (samples[i] < 0) ? -(int32_t)samples[i] : (int32_t)samples[i];
    if (a >= BOARD_VOX_THRESHOLD_ABS) {
      hit++;
    }
  }

  const uint32_t now = to_ms_since_boot(get_absolute_time());

  if (hit >= BOARD_VOX_MIN_ACTIVE_SAMPLES) {
    s_active_run += hit;
    s_last_activity_ms = now;
    if (!s_ptt_on && s_active_run >= BOARD_VOX_MIN_ACTIVE_SAMPLES) {
      s_ptt_on = true;
      ptt_set_active(true);
    }
  } else {
    s_active_run = 0;
  }
}

void vox_on_stream_stop(void) {
  if (!s_enabled) {
    return;
  }
  s_ptt_on = false;
  s_active_run = 0;
  s_last_activity_ms = 0;
  ptt_force_inactive();
}

void vox_task(void) {
  if (!s_enabled || !s_ptt_on) {
    return;
  }

  const uint32_t now = to_ms_since_boot(get_absolute_time());
  if ((now - s_last_activity_ms) >= BOARD_VOX_HANG_MS) {
    s_ptt_on = false;
    s_active_run = 0;
    ptt_set_active(false);
  }
}
