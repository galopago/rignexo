#ifndef MODE_STORE_H
#define MODE_STORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DEVICE_MODE_COMPOSITE = 1,
  DEVICE_MODE_AUDIO_VOX = 2,
  DEVICE_MODE_COMPOSITE_CAT = 3,
} device_mode_t;

/* Load/validate/heal/advance once; at most one flash write. Call before tusb_init. */
device_mode_t mode_boot_resolve(void);

device_mode_t device_mode_get(void);

bool device_mode_is_composite(void);
bool device_mode_is_audio_vox(void);
bool device_mode_is_composite_cat(void);

device_mode_t device_mode_next(device_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
