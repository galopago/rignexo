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

/* D40.2 mode-4 packet LED: baseline 150/150; FCS → long OFF 250/50. */
#define BOARD_MODE4_LED_TICK_MS           300U
#define BOARD_MODE4_LED_OFF_BASELINE_MS   150U /* same as modes 1–3 */
#define BOARD_MODE4_LED_GAP_BASELINE_MS   150U
#define BOARD_MODE4_LED_OFF_PKT_MS        250U /* D40.2: +10 ms vs D40.1 */
#define BOARD_MODE4_LED_GAP_PKT_MS         50U /* = TICK − OFF_PKT; keep 4 edges */
/* STAT window echo only — amplitude never promotes packet blink (D39/D40). */
#define BOARD_MODE4_AF_PP_GOOD            120
#define BOARD_MODE4_AF_PP_BAD              80
#define BOARD_MODE4_AF_PP_ACTIVITY         40
#define BOARD_MODE4_AF_UPDATE_MS           200U
/* Keep long-OFF after FCS, then baseline. */
#define BOARD_MODE4_AF_HOLD_MS            3000U

/* Boot mode-switch anti-glitch. */
#define BOARD_MODE_SWITCH_SETTLE_MS     15U
#define BOARD_MODE_SWITCH_HOLD_MS       75U

/* AUDIO_VOX speaker-path detector (tunable on bench). */
#define BOARD_VOX_THRESHOLD_ABS         800
#define BOARD_VOX_HANG_MS               300U
#define BOARD_VOX_MIN_ACTIVE_SAMPLES    8U

#define BOARD_PWM_WRAP                  255U
#define BOARD_PWM_CLKDIV                4.0f

/* APRS TNC / KISS defaults (×10 ms for TXDELAY/TXTAIL/SlotTime). */
#define BOARD_KISS_TXDELAY              30U
#define BOARD_KISS_PERSIST              63U
#define BOARD_KISS_SLOTTIME             10U
#define BOARD_KISS_TXTAIL               5U
/* TX PWM sample clock (RIGNEXO-owned path). */
#define BOARD_AFSK_SAMPLE_RATE_HZ       9600U
#define BOARD_AFSK_MARK_HZ              1200U
#define BOARD_AFSK_SPACE_HZ             2200U
#define BOARD_AFSK_BAUD                 1200U

/*
 * D31: A+ RX sample clock = 1200 × 20. Separate from TX (9600).
 * D33: D32 WEAK reverted — keep D31 rate / N only.
 */
#define BOARD_AFSK_RX_SAMPLE_RATE_HZ    24000U

/* Legacy DCD defines (docs); live DCD from afsk_aplus (STAT/CSMA only). */
#define BOARD_AFSK_DCD_THRESHOLD        400
#define BOARD_AFSK_DCD_HANG_MS          100U

/* A+ DCD on normalized mark+space envelope (STAT/CSMA — does not gate HDLC). */
#define BOARD_AFSK_APLUS_DCD_OPEN       0.02f
#define BOARD_AFSK_APLUS_DCD_CLOSE      0.01f

#ifndef AFSK_APLUS_N_SLICERS
#define AFSK_APLUS_N_SLICERS            5
#endif

#endif
