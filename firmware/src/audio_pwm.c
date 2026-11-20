#include "audio_pwm.h"

#include "board_config.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#define AUDIO_PWM_RING_CAPACITY 4096U

static int16_t s_ring[AUDIO_PWM_RING_CAPACITY];
static volatile uint32_t s_write_idx;
static volatile uint32_t s_read_idx;
static struct repeating_timer s_timer;
static bool s_running;
static uint s_pwm_slice;
static uint s_pwm_channel;

static uint16_t pcm_to_pwm_level(int16_t sample) {
    const int32_t centered = ((int32_t)sample + 32768) * (int32_t)BOARD_PWM_WRAP;
    return (uint16_t)(centered / 65536);
}

static bool audio_pwm_timer_cb(struct repeating_timer *rt) {
    (void)rt;

    int16_t sample = 0;
    if (s_read_idx != s_write_idx) {
        sample = s_ring[s_read_idx];
        s_read_idx = (s_read_idx + 1U) % AUDIO_PWM_RING_CAPACITY;
    }

    pwm_set_chan_level(s_pwm_slice, s_pwm_channel, pcm_to_pwm_level(sample));
    return true;
}

void audio_pwm_init(void) {
    gpio_set_function(BOARD_RADIO_AUDIO_OUT_GPIO, GPIO_FUNC_PWM);
    s_pwm_slice = pwm_gpio_to_slice_num(BOARD_RADIO_AUDIO_OUT_GPIO);
    s_pwm_channel = pwm_gpio_to_channel(BOARD_RADIO_AUDIO_OUT_GPIO);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, BOARD_PWM_CLKDIV);
    pwm_config_set_wrap(&cfg, BOARD_PWM_WRAP);
    pwm_init(s_pwm_slice, &cfg, true);
    pwm_set_chan_level(s_pwm_slice, s_pwm_channel, BOARD_PWM_WRAP / 2U);

    s_write_idx = 0;
    s_read_idx = 0;
    s_running = false;
}

void audio_pwm_start(void) {
    if (s_running) {
        return;
    }

    s_write_idx = 0;
    s_read_idx = 0;

    const int64_t delay_us = -((int64_t)1000000 / (int64_t)BOARD_AUDIO_SAMPLE_RATE_HZ);
    s_running = add_repeating_timer_us(delay_us, audio_pwm_timer_cb, NULL, &s_timer);
}

void audio_pwm_stop(void) {
    if (!s_running) {
        pwm_set_chan_level(s_pwm_slice, s_pwm_channel, BOARD_PWM_WRAP / 2U);
        return;
    }

    cancel_repeating_timer(&s_timer);
    s_running = false;
    s_write_idx = 0;
    s_read_idx = 0;
    pwm_set_chan_level(s_pwm_slice, s_pwm_channel, BOARD_PWM_WRAP / 2U);
}

size_t audio_pwm_write(const int16_t *src, size_t count) {
    size_t written = 0;

    while (written < count) {
        const uint32_t next = (s_write_idx + 1U) % AUDIO_PWM_RING_CAPACITY;
        if (next == s_read_idx) {
            break;
        }

        s_ring[s_write_idx] = src[written++];
        s_write_idx = next;
    }

    return written;
}
