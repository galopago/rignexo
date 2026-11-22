#ifndef KISS_CDC_H
#define KISS_CDC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void kiss_cdc_init(void);
void kiss_cdc_feed(const uint8_t *data, size_t len);

/* Emit one KISS data frame (port 0) to the host; payload has no FCS. */
void kiss_cdc_send_frame(const uint8_t *payload, size_t len);

uint8_t kiss_cdc_txdelay(void);
uint8_t kiss_cdc_txtail(void);
uint8_t kiss_cdc_persist(void);
uint8_t kiss_cdc_slottime(void);
uint8_t kiss_cdc_fullduplex(void);

/* D25 L3 tallies. */
uint32_t kiss_cdc_dbg_kiss_ok(void);
uint32_t kiss_cdc_dbg_kiss_drop(void);
void kiss_cdc_dbg_note_drop(void);
void kiss_cdc_dbg_clear(void);

#ifdef __cplusplus
}
#endif

#endif
