#ifndef AUDIO_PWM_H
#define AUDIO_PWM_H

#include <stddef.h>
#include <stdint.h>

void audio_pwm_init(void);
void audio_pwm_start(void);
void audio_pwm_stop(void);
size_t audio_pwm_write(const int16_t *src, size_t count);

#endif
