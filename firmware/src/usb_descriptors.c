/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Jerzy Kasenberg
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

#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "mode_store.h"

/* TinyUSB-style PID bitmap + personality bits: VOX=6, CAT=7, TNC=8. */
#define PID_MAP(itf, n)  ((CFG_TUD_##itf) ? (1 << (n)) : 0)
#define USB_PID_COMPOSITE (0x4000 | PID_MAP(CDC, 0) | PID_MAP(MSC, 1) | PID_MAP(HID, 2) | \
    PID_MAP(MIDI, 3) | PID_MAP(AUDIO, 4) | PID_MAP(VENDOR, 5))
#define USB_PID_AUDIO_VOX (0x4000 | PID_MAP(AUDIO, 4) | (1 << 6))
#define USB_PID_COMPOSITE_CAT (USB_PID_COMPOSITE | (1 << 7))
#define USB_PID_APRS_TNC (0x4000 | PID_MAP(CDC, 0) | (1 << 8))

#define CONFIG_TOTAL_LEN_COMPOSITE \
    (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_HEADSET_STEREO_DESC_LEN + \
     CFG_TUD_CDC * TUD_CDC_DESC_LEN)
#define CONFIG_TOTAL_LEN_AUDIO_ONLY \
    (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_HEADSET_STEREO_DESC_LEN)
#define CONFIG_TOTAL_LEN_CDC_ONLY \
    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)

/* APRS_TNC: CDC starts at interface 0 (no UAC2). */
#define ITF_NUM_CDC_TNC       0
#define ITF_NUM_TOTAL_CDC_ONLY 2

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  #define EPNUM_AUDIO_IN    0x03
  #define EPNUM_AUDIO_OUT   0x03
  #define EPNUM_CDC_NOTIF   0x84
  #define EPNUM_CDC_OUT     0x05
  #define EPNUM_CDC_IN      0x85
#elif CFG_TUSB_MCU == OPT_MCU_NRF5X
  #define EPNUM_AUDIO_IN    0x08
  #define EPNUM_AUDIO_OUT   0x08
  #define EPNUM_CDC_NOTIF   0x81
  #define EPNUM_CDC_OUT     0x02
  #define EPNUM_CDC_IN      0x82
#elif CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  #if TU_CHECK_MCU(OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX32690, OPT_MCU_MAX78002)
    #define EPNUM_AUDIO_OUT   0x0A
    #define EPNUM_AUDIO_IN    0x0B
    #define EPNUM_CDC_NOTIF   0x83
    #define EPNUM_CDC_OUT     0x08
    #define EPNUM_CDC_IN      0x89
  #else
    #define EPNUM_AUDIO_IN    0x01
    #define EPNUM_AUDIO_OUT   0x02
    #define EPNUM_CDC_NOTIF   0x83
    #define EPNUM_CDC_OUT     0x04
    #define EPNUM_CDC_IN      0x85
  #endif
#else
  #define EPNUM_AUDIO_IN    0x01
  #define EPNUM_AUDIO_OUT   0x01
  #define EPNUM_CDC_NOTIF   0x83
  #define EPNUM_CDC_OUT     0x04
  #define EPNUM_CDC_IN      0x84
#endif

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
static tusb_desc_device_t const desc_device_composite = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = USB_PID_COMPOSITE,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static tusb_desc_device_t const desc_device_audio_vox = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = USB_PID_AUDIO_VOX,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static tusb_desc_device_t const desc_device_composite_cat = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = USB_PID_COMPOSITE_CAT,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static tusb_desc_device_t const desc_device_aprs_tnc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = USB_PID_APRS_TNC,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x07, /* "RIGNEXO KISS TNC" */
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
  if (device_mode_is_audio_vox()) {
    return (uint8_t const *)&desc_device_audio_vox;
  }
  if (device_mode_is_composite_cat()) {
    return (uint8_t const *)&desc_device_composite_cat;
  }
  if (device_mode_is_aprs_tnc()) {
    return (uint8_t const *)&desc_device_aprs_tnc;
  }
  return (uint8_t const *)&desc_device_composite;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
static uint8_t const desc_fs_configuration_composite[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_COMPOSITE, 0, CONFIG_TOTAL_LEN_COMPOSITE, 0x00, 100),
    TUD_AUDIO_HEADSET_STEREO_DESCRIPTOR(2, EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

static uint8_t const desc_fs_configuration_audio_vox[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_AUDIO_ONLY, 0, CONFIG_TOTAL_LEN_AUDIO_ONLY, 0x00, 100),
    TUD_AUDIO_HEADSET_STEREO_DESCRIPTOR(2, EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80)
};

static uint8_t const desc_fs_configuration_cdc_only[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_CDC_ONLY, 0, CONFIG_TOTAL_LEN_CDC_ONLY, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_TNC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

#if TUD_OPT_HIGH_SPEED
static uint8_t const desc_hs_configuration_composite[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_COMPOSITE, 0, CONFIG_TOTAL_LEN_COMPOSITE, 0x00, 100),
    TUD_AUDIO_HEADSET_STEREO_DESCRIPTOR(2, EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512)
};

static uint8_t const desc_hs_configuration_audio_vox[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_AUDIO_ONLY, 0, CONFIG_TOTAL_LEN_AUDIO_ONLY, 0x00, 100),
    TUD_AUDIO_HEADSET_STEREO_DESCRIPTOR(2, EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80)
};

static uint8_t const desc_hs_configuration_cdc_only[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_CDC_ONLY, 0, CONFIG_TOTAL_LEN_CDC_ONLY, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_TNC, 6, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512)
};

static uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN_COMPOSITE];

static tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0100,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00
};

uint8_t const *tud_descriptor_device_qualifier_cb(void) {
  return (uint8_t const *)&desc_device_qualifier;
}

uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  (void)index;

  const uint8_t *src;
  uint16_t len;

  if (device_mode_is_audio_vox()) {
    src = (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration_audio_vox
                                              : desc_hs_configuration_audio_vox;
    len = CONFIG_TOTAL_LEN_AUDIO_ONLY;
  } else if (device_mode_is_aprs_tnc()) {
    src = (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration_cdc_only
                                              : desc_hs_configuration_cdc_only;
    len = CONFIG_TOTAL_LEN_CDC_ONLY;
  } else {
    src = (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration_composite
                                              : desc_hs_configuration_composite;
    len = CONFIG_TOTAL_LEN_COMPOSITE;
  }

  memcpy(desc_other_speed_config, src, len);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
  return desc_other_speed_config;
}
#endif

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
#if TUD_OPT_HIGH_SPEED
  if (device_mode_is_audio_vox()) {
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration_audio_vox
                                                : desc_fs_configuration_audio_vox;
  }
  if (device_mode_is_aprs_tnc()) {
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration_cdc_only
                                                : desc_fs_configuration_cdc_only;
  }
  return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration_composite
                                              : desc_fs_configuration_composite;
#else
  if (device_mode_is_audio_vox()) {
    return desc_fs_configuration_audio_vox;
  }
  if (device_mode_is_aprs_tnc()) {
    return desc_fs_configuration_cdc_only;
  }
  return desc_fs_configuration_composite;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 },
  "RIGNEXO",
  "RIGNEXO interface",
  NULL,
  "RIGNEXO Speakers",
  "RIGNEXO Microphone",
  "RIGNEXO CDC",
  "RIGNEXO KISS TNC",
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  size_t chr_count;

  switch (index) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
        return NULL;
      }

      const char *str = string_desc_arr[index];
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
      if (chr_count > max_count) {
        chr_count = max_count;
      }

      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
