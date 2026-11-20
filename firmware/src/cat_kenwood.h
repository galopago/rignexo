#ifndef CAT_KENWOOD_H
#define CAT_KENWOOD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cat_kenwood_init(void);

/* Feed CDC RX bytes; replies are written via tud_cdc_write. */
void cat_kenwood_feed(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
