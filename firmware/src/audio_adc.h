#ifndef AUDIO_ADC_H
#define AUDIO_ADC_H

#include <stddef.h>
#include <stdint.h>

void audio_adc_init(void);
void audio_adc_start(void);
void audio_adc_stop(void);
size_t audio_adc_read(int16_t *dst, size_t count);

#endif
