#ifndef MODE_LED_PIO_H
#define MODE_LED_PIO_H

#include "mode_store.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Claim PIO1 SM0; start forever pattern. No periodic CPU work for modes 1–3. */
void mode_led_pio_start(device_mode_t mode);

/*
 * Mode 4 only (D40.2): true = packet blink (long OFF 250); false = baseline 150/150.
 * No-op in modes 1–3. Safe to call from afsk_task (not every ADC sample).
 */
void mode_led_pio_set_af_good(bool packet_ok);

/* D38: last OFF/GAP/TICK ms loaded into mode-4 PIO (0 if not mode 4). */
void mode_led_pio_af_dbg_get(uint32_t *off_ms, uint32_t *gap_ms,
                             uint32_t *tick_ms, bool *good);

#ifdef __cplusplus
}
#endif

#endif
