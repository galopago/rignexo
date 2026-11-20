/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jerzy Kasenberg
 * Copyright (c) 2022 Angel Molina <angelmolinu@gmail.com>
 * Copyright (c) 2023 Dhiru Kholia <dhiru.kholia@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <string.h>

#include "bsp/board_api.h"
#include "audio_adc.h"
#include "audio_pwm.h"
#include "cat_kenwood.h"
#include "mode_led_pio.h"
#include "mode_store.h"
#include "ptt.h"
#include "tusb.h"
#include "common.h"
#include "vox.h"

extern uint32_t blink_interval_ms;

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  /* Resolve persistent mode before any USB init (no runtime re-enumeration). */
  device_mode_t mode = mode_boot_resolve();
  /* GPIO0 = MODE_LED exclusive after this; do not call stdio_init_all / UART0. */
  mode_led_pio_start(mode);

  ptt_init();
  audio_pwm_init();
  audio_adc_init();
  vox_init();
  if (device_mode_is_composite_cat()) {
    cat_kenwood_init();
  }

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  TU_LOG1("RIGNEXO mode %u running\r\n", (unsigned)mode);

  while (1)
  {
    tud_task(); // TinyUSB device task
    led_blinking_task();
    audio_task();
    vox_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
  vox_on_stream_stop();
  ptt_force_inactive();
  audio_pwm_stop();
  audio_adc_stop();
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}
