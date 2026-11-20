#ifndef MODE_LED_PIO_H
#define MODE_LED_PIO_H

#include "mode_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Claim PIO1 SM0; start forever pattern. No periodic CPU work after return. */
void mode_led_pio_start(device_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
