#include "mode_store.h"

#include "board_config.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <string.h>

#define MODE_STORE_MAGIC          0x52474E58u /* 'RGNX' */
#define MODE_STORE_FLASH_OFFSET   (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef struct {
  uint32_t magic;
  uint8_t mode;
  uint8_t reserved[3];
  uint32_t checksum;
} mode_store_record_t;

static device_mode_t s_mode = DEVICE_MODE_COMPOSITE;

static uint32_t mode_store_checksum(const mode_store_record_t *rec) {
  return rec->magic ^ (uint32_t)rec->mode ^ 0xA5A5A5A5u ^
         ((uint32_t)rec->reserved[0] << 16) ^
         ((uint32_t)rec->reserved[1] << 8) ^
         (uint32_t)rec->reserved[2];
}

static bool mode_is_known(uint8_t mode) {
  return mode == (uint8_t)DEVICE_MODE_COMPOSITE ||
         mode == (uint8_t)DEVICE_MODE_AUDIO_VOX ||
         mode == (uint8_t)DEVICE_MODE_COMPOSITE_CAT;
}

device_mode_t device_mode_next(device_mode_t mode) {
  if (mode == DEVICE_MODE_COMPOSITE) {
    return DEVICE_MODE_AUDIO_VOX;
  }
  if (mode == DEVICE_MODE_AUDIO_VOX) {
    return DEVICE_MODE_COMPOSITE_CAT;
  }
  return DEVICE_MODE_COMPOSITE;
}

static bool mode_store_read(mode_store_record_t *out) {
  const mode_store_record_t *flash_rec =
      (const mode_store_record_t *)(XIP_BASE + MODE_STORE_FLASH_OFFSET);
  memcpy(out, flash_rec, sizeof(*out));
  if (out->magic != MODE_STORE_MAGIC) {
    return false;
  }
  if (out->checksum != mode_store_checksum(out)) {
    return false;
  }
  if (!mode_is_known(out->mode)) {
    return false;
  }
  return true;
}

static bool mode_store_same_as(device_mode_t mode) {
  mode_store_record_t existing;
  if (!mode_store_read(&existing)) {
    return false;
  }
  return existing.mode == (uint8_t)mode;
}

static void mode_store_write(device_mode_t mode) {
  mode_store_record_t rec;
  uint8_t page[FLASH_PAGE_SIZE];

  memset(&rec, 0, sizeof(rec));
  rec.magic = MODE_STORE_MAGIC;
  rec.mode = (uint8_t)mode;
  rec.checksum = mode_store_checksum(&rec);

  memset(page, 0xFF, sizeof(page));
  memcpy(page, &rec, sizeof(rec));

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(MODE_STORE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(MODE_STORE_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
  restore_interrupts(ints);
}

static bool mode_switch_is_active(void) {
  const bool level = gpio_get(BOARD_MODE_SWITCH_GPIO);
  return level == (bool)BOARD_MODE_SWITCH_ACTIVE_LEVEL;
}

static bool mode_switch_held_for_window(void) {
  sleep_ms(BOARD_MODE_SWITCH_SETTLE_MS);

  absolute_time_t deadline = make_timeout_time_ms(BOARD_MODE_SWITCH_HOLD_MS);
  while (!time_reached(deadline)) {
    if (!mode_switch_is_active()) {
      return false;
    }
    tight_loop_contents();
  }
  return mode_switch_is_active();
}

device_mode_t mode_boot_resolve(void) {
  gpio_init(BOARD_MODE_SWITCH_GPIO);
  gpio_set_dir(BOARD_MODE_SWITCH_GPIO, GPIO_IN);
  /* Match mainboard: external pull-down, switch to 3V3 (active high). */
  gpio_pull_down(BOARD_MODE_SWITCH_GPIO);

  mode_store_record_t loaded;
  const bool valid = mode_store_read(&loaded);
  device_mode_t effective =
      valid ? (device_mode_t)loaded.mode : DEVICE_MODE_COMPOSITE;
  bool write_needed = !valid;

  if (mode_switch_held_for_window()) {
    effective = device_mode_next(effective);
    write_needed = true;
  }

  if (write_needed && !mode_store_same_as(effective)) {
    mode_store_write(effective);
  }

  s_mode = effective;
  return s_mode;
}

device_mode_t device_mode_get(void) {
  return s_mode;
}

bool device_mode_is_composite(void) {
  return s_mode == DEVICE_MODE_COMPOSITE;
}

bool device_mode_is_audio_vox(void) {
  return s_mode == DEVICE_MODE_AUDIO_VOX;
}

bool device_mode_is_composite_cat(void) {
  return s_mode == DEVICE_MODE_COMPOSITE_CAT;
}
