#ifndef AFSK_BELL202_H
#define AFSK_BELL202_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void afsk_init(void);
void afsk_shutdown(void);

/* Queue AX.25 payload without FCS (max 512). Drops newest if queue full while busy. */
bool afsk_tx_queue_frame(const uint8_t *payload, size_t len);

bool afsk_tx_busy(void);

/* True while channel energy / demod activity looks busy (CSMA). */
bool afsk_dcd(void);

/* Call from main loop: CSMA waits, drain RX→KISS, D25 STAT dump. */
void afsk_task(void);

/* D25: clear since-clear counters / latches; request ASCII dump (done in afsk_task). */
void afsk_rx_debug_clear(void);
void afsk_rx_debug_request_dump(void);

#ifdef __cplusplus
}
#endif

#endif
