#include "audio_adc.h"

#include "board_config.h"

#include "hardware/adc.h"
#include "pico/stdlib.h"

#define AUDIO_ADC_RING_CAPACITY 4096U

static int16_t s_ring[AUDIO_ADC_RING_CAPACITY];
static volatile uint32_t s_write_idx;
static volatile uint32_t s_read_idx;
static struct repeating_timer s_timer;
static bool s_running;

static int16_t adc_raw_to_pcm16(uint16_t raw) {
    return (int16_t)(((int32_t)raw - 2048) << 4);
}

static bool audio_adc_timer_cb(struct repeating_timer *rt) {
    (void)rt;

    const uint32_t next = (s_write_idx + 1U) % AUDIO_ADC_RING_CAPACITY;
    if (next == s_read_idx) {
        (void)adc_read();
        return true;
    }

    s_ring[s_write_idx] = adc_raw_to_pcm16(adc_read());
    s_write_idx = next;
    return true;
}

void audio_adc_init(void) {
    adc_init();
    adc_gpio_init(BOARD_RADIO_AUDIO_IN_GPIO);
    adc_select_input(BOARD_ADC_INPUT_INDEX);

    s_write_idx = 0;
    s_read_idx = 0;
    s_running = false;
}

void audio_adc_start(void) {
    if (s_running) {
        return;
    }

    s_write_idx = 0;
    s_read_idx = 0;
    adc_select_input(BOARD_ADC_INPUT_INDEX);

    const int64_t delay_us = -((int64_t)1000000 / (int64_t)BOARD_AUDIO_SAMPLE_RATE_HZ);
    s_running = add_repeating_timer_us(delay_us, audio_adc_timer_cb, NULL, &s_timer);
}

void audio_adc_stop(void) {
    if (!s_running) {
        return;
    }

    cancel_repeating_timer(&s_timer);
    s_running = false;
    s_write_idx = 0;
    s_read_idx = 0;
}

size_t audio_adc_read(int16_t *dst, size_t count) {
    size_t read = 0;

    while (read < count) {
        if (s_read_idx == s_write_idx) {
            break;
        }

        dst[read++] = s_ring[s_read_idx];
        s_read_idx = (s_read_idx + 1U) % AUDIO_ADC_RING_CAPACITY;
    }

    return read;
}
