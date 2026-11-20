#ifndef VOX_H
#define VOX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void vox_init(void);
void vox_process_pcm16(const int16_t *samples, size_t count);
void vox_on_stream_stop(void);
void vox_task(void);

#ifdef __cplusplus
}
#endif

#endif
