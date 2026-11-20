#include "ptt.h"

#include "board_config.h"

#include "hardware/gpio.h"

static bool s_active;

static void ptt_apply(bool transmit_on) {
    s_active = transmit_on;
    gpio_put(BOARD_RADIO_PTT_OUT_GPIO,
             transmit_on ? BOARD_PTT_ACTIVE_LEVEL : !BOARD_PTT_ACTIVE_LEVEL);
    gpio_put(BOARD_TX_INDICATOR_GPIO,
             transmit_on ? BOARD_TX_INDICATOR_ACTIVE_LEVEL : !BOARD_TX_INDICATOR_ACTIVE_LEVEL);
}

void ptt_init(void) {
    gpio_init(BOARD_RADIO_PTT_OUT_GPIO);
    gpio_set_dir(BOARD_RADIO_PTT_OUT_GPIO, GPIO_OUT);

    gpio_init(BOARD_TX_INDICATOR_GPIO);
    gpio_set_dir(BOARD_TX_INDICATOR_GPIO, GPIO_OUT);

    ptt_apply(false);
}

void ptt_set_active(bool transmit_on) {
    ptt_apply(transmit_on);
}

void ptt_force_inactive(void) {
    ptt_apply(false);
}

bool ptt_is_active(void) {
    return s_active;
}
