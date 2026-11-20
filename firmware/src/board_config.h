#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

/*
 * Shared GPIO map for Waveshare RP2350-Zero (primary) and RP2040-Zero
 * (identical pins, backward compatible). One header for both CHIP builds.
 */
#define BOARD_RADIO_AUDIO_IN_GPIO       28
#define BOARD_RADIO_AUDIO_OUT_GPIO      2
#define BOARD_RADIO_PTT_OUT_GPIO        14  /* PTT relay */

#define BOARD_ADC_INPUT_INDEX           2  /* GPIO28 = ADC2 */

#define BOARD_AUDIO_SAMPLE_RATE_HZ      48000U
#define BOARD_AUDIO_BITS                16

#define BOARD_PTT_ACTIVE_LEVEL          1
#define BOARD_PTT_USE_RTS_LINE          1

#define BOARD_TX_INDICATOR_GPIO         27  /* PTT LED */
#define BOARD_TX_INDICATOR_ACTIVE_LEVEL 1

#define BOARD_MODE_SWITCH_GPIO          15
#define BOARD_MODE_LED_GPIO             0
/* Mainboard: +3V3 — SW — GPIO15 — R pull-down — GND (pressed = high). */
#define BOARD_MODE_SWITCH_ACTIVE_LEVEL  1   /* active high */
#define BOARD_MODE_LED_ACTIVE_LEVEL     1   /* active high */

/* Mode LED PIO timing (within architecture guidance band). */
#define BOARD_MODE_LED_OFF_MS           150U
#define BOARD_MODE_LED_GAP_MS           150U
#define BOARD_MODE_LED_QUIET_ON_MS      800U

/* Boot mode-switch anti-glitch. */
#define BOARD_MODE_SWITCH_SETTLE_MS     15U
#define BOARD_MODE_SWITCH_HOLD_MS       75U

/* AUDIO_VOX speaker-path detector (tunable on bench). */
#define BOARD_VOX_THRESHOLD_ABS         800
#define BOARD_VOX_HANG_MS               300U
#define BOARD_VOX_MIN_ACTIVE_SAMPLES    8U

#define BOARD_PWM_WRAP                  255U
#define BOARD_PWM_CLKDIV                4.0f

#endif
