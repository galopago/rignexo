/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022 Angel Molina (angelmolinu@gmail.com)
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

#include "bsp/board_api.h"
#include "tusb.h"
#include "common.h"
#include "cat_kenwood.h"
#include "kiss_cdc.h"
#include "mode_store.h"

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
  uint8_t buf[64];
  uint32_t count;

  if (device_mode_is_audio_vox()) {
    return;
  }

  if (tud_cdc_available() == 0) {
    return;
  }

  count = tud_cdc_n_read(itf, buf, sizeof(buf));
  if (count == 0) {
    return;
  }

  if (device_mode_is_aprs_tnc()) {
    kiss_cdc_feed(buf, count);
    return;
  }

  if (device_mode_is_composite_cat()) {
    cat_kenwood_feed(buf, count);
    return;
  }

  if (!device_mode_is_composite()) {
    return;
  }

  /* COMPOSITE mode 1: byte echo (legacy bench check). */
  tud_cdc_n_write(itf, buf, count);
  tud_cdc_n_write_flush(itf);
  board_led_write(0);
  board_delay(50);
  board_led_write(1);
  board_delay(50);
  board_led_write(0);
}
