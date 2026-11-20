#include "mode_led_pio.h"

#include "board_config.h"
#include "mode_led.pio.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

/* Prefer PIO1 SM0 — leave PIO0 free for other peripherals. */
#define MODE_LED_PIO       pio1
#define MODE_LED_PIO_SM    0

static uint32_t ms_to_loops(uint32_t ms) {
  const uint32_t hz = clock_get_hz(clk_sys);
  return (hz / 1000u) * ms;
}

static void load_delay_regs(PIO pio, uint sm, uint32_t quiet_loops, uint32_t short_loops) {
  pio_sm_put(pio, sm, quiet_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_isr, pio_osr));

  pio_sm_put(pio, sm, short_loops);
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_mov(pio_y, pio_osr));
}

void mode_led_pio_start(device_mode_t mode) {
  PIO pio = MODE_LED_PIO;
  const uint sm = MODE_LED_PIO_SM;
  const uint pin = BOARD_MODE_LED_GPIO;

  /* Architect D15 / Option A: GPIO0 is MODE_LED only after this call.
   * Callers MUST NOT remux the pin (no stdio_init_all / UART0 TX on 0). */
  const pio_program_t *program;
  pio_sm_config cfg;

  if (mode == DEVICE_MODE_COMPOSITE_CAT) {
    program = &mode_led_3pulse_program;
  } else if (mode == DEVICE_MODE_AUDIO_VOX) {
    program = &mode_led_2pulse_program;
  } else {
    program = &mode_led_1pulse_program;
  }

  const uint offset = pio_add_program(pio, program);

  pio_gpio_init(pio, pin);
  gpio_set_function(pin, GPIO_FUNC_PIO1);
  pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

  if (mode == DEVICE_MODE_COMPOSITE_CAT) {
    cfg = mode_led_3pulse_program_get_default_config(offset);
  } else if (mode == DEVICE_MODE_AUDIO_VOX) {
    cfg = mode_led_2pulse_program_get_default_config(offset);
  } else {
    cfg = mode_led_1pulse_program_get_default_config(offset);
  }
  sm_config_set_set_pins(&cfg, pin, 1);
  sm_config_set_clkdiv(&cfg, 1.0f);

  pio_sm_init(pio, sm, offset, &cfg);

  const uint32_t quiet = ms_to_loops(BOARD_MODE_LED_QUIET_ON_MS);
  const uint32_t pulse = ms_to_loops(BOARD_MODE_LED_OFF_MS);
  (void)BOARD_MODE_LED_GAP_MS; /* gap uses same short delay as OFF (150 ms). */
  load_delay_regs(pio, sm, quiet, pulse);

  /* Force initial ON level before the SM runs. */
  if (BOARD_MODE_LED_ACTIVE_LEVEL) {
    pio_sm_set_pins_with_mask(pio, sm, 1u << pin, 1u << pin);
  } else {
    pio_sm_set_pins_with_mask(pio, sm, 0, 1u << pin);
  }

  pio_sm_set_enabled(pio, sm, true);
}
