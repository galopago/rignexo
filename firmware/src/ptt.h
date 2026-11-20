#ifndef PTT_H
#define PTT_H

#include <stdbool.h>

void ptt_init(void);
void ptt_set_active(bool transmit_on);
void ptt_force_inactive(void);

#endif
