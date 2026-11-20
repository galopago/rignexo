#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "mode_store.h"
#include "ptt.h"

// Strong symbol: do not include TinyUSB headers (TU_ATTR_WEAK on the prototype).
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;

    if (!device_mode_is_composite()) {
        return;
    }

#if BOARD_PTT_USE_RTS_LINE
    (void)dtr;
    ptt_set_active(rts);
#else
    (void)rts;
    ptt_set_active(dtr);
#endif
}
